#include "data/database/workspace_store.h"
#include "next/application/sub_prep_roster_output_source_query.h"
#include "next/application/sub_prep_weekday.h"
#include "next/platform/workspace_store_gateway.h"
#include "sql_script_runner.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <string>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const int id)
{
    return *Domain::ClassId::fromString(std::to_string(id));
}

Application::WorkspaceLocation location(const QString& path)
{
    return Application::WorkspaceLocation(
        path.toUtf8().toStdString());
}

bool seedWorkspace(const QString& path, const QString& marker, QString* error)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }

    QFile fixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/workspaces/large_startup.sql"));
    if (!fixture.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("Could not read large_startup.sql.");
        return false;
    }
    if (!Test::executeSqlScript(
            store.database(), QString::fromUtf8(fixture.readAll()), error))
    {
        return false;
    }

    QSqlQuery classInfo(store.database());
    classInfo.prepare(QStringLiteral(
        "UPDATE class_info SET notes=? WHERE class_id=1"));
    classInfo.addBindValue(marker + QStringLiteral(" class notes"));
    if (!classInfo.exec())
    {
        *error = classInfo.lastError().text();
        return false;
    }

    QSqlQuery teacher(store.database());
    teacher.prepare(QStringLiteral(
        "UPDATE teachers SET teacher_en=?, preferred_name=? WHERE id=1"));
    teacher.addBindValue(marker + QStringLiteral(" Teacher"));
    teacher.addBindValue(marker + QStringLiteral(" Teacher"));
    if (!teacher.exec())
    {
        *error = teacher.lastError().text();
        return false;
    }
    return true;
}

bool executeOnSeparateConnection(const QString& path, const QString& sql)
{
    const QString connectionName = QStringLiteral("sub-prep-gateway-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            executed = query.exec(sql);
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}

Application::SubPrepScheduleScopeRequest scopeFor(
    const std::vector<Domain::ClassId>& ids)
{
    return {
        .visibleClassIds = ids,
        .selectedDays = {
            Application::SubPrepWeekday::Monday,
            Application::SubPrepWeekday::Tuesday,
            Application::SubPrepWeekday::Wednesday,
            Application::SubPrepWeekday::Thursday,
            Application::SubPrepWeekday::Friday
        },
        .mode = Application::ScheduleViewMode::Regular
    };
}

} // namespace

class WorkspaceStoreSubPrepGatewayTests final : public QObject
{
    Q_OBJECT

private slots:
    void summaryDetailsAndOutputReadFromTheCurrentPrivateStore();
    void unopenedClosedInvalidScopeAndRepositoryFailuresAreReported();
};

void WorkspaceStoreSubPrepGatewayTests::
summaryDetailsAndOutputReadFromTheCurrentPrivateStore()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString workspaceA = directory.filePath(QStringLiteral("a.tps"));
    const QString workspaceB = directory.filePath(QStringLiteral("b.tps"));
    QString fixtureError;
    QVERIFY2(seedWorkspace(workspaceA, QStringLiteral("Workspace A"),
                           &fixtureError), qPrintable(fixtureError));
    QVERIFY2(seedWorkspace(workspaceB, QStringLiteral("Workspace B"),
                           &fixtureError), qPrintable(fixtureError));

    Platform::WorkspaceStoreGateway gateway;
    const auto openedA = gateway.openWorkspace({.location = location(workspaceA)});
    QVERIFY(openedA);

    const auto summaryA = gateway.readSubPrepSummaries(scopeFor({classId(1)}));
    QVERIFY(summaryA);
    QCOMPARE(summaryA.value().classes().size(), std::size_t(1));
    QCOMPARE(summaryA.value().classes().front().id, classId(1));

    const auto detailsA = gateway.readSubPrepSelectedDetails(classId(1));
    QVERIFY(detailsA);
    QCOMPARE(detailsA.value().classNotes, std::string("Workspace A class notes"));
    QCOMPARE(detailsA.value().teacherDisplayName,
             std::string("Workspace A Teacher"));

    auto rosterPort = gateway.createSubPrepRosterOutputSourceReadPort();
    QVERIFY(rosterPort);
    const Application::SubPrepRosterOutputSourceRequest rosterRequest{
        .selectedClassIds = {classId(1)},
        .selectedDays = {Application::SubPrepWeekday::Monday},
        .mode = Application::ScheduleViewMode::Regular,
        .selectedExtraColumns = {"School"}
    };
    const auto roster = Application::SubPrepRosterOutputSourceQuery(
        *rosterPort).execute(rosterRequest);
    QVERIFY(roster);
    QCOMPARE(roster.value().classes().size(), std::size_t(1));
    QCOMPARE(roster.value().classes().front().classroomName,
             std::string("Heavy Fixture Class 001"));
    QVERIFY(!roster.value().classes().front().rosterRows.empty());

    const auto closedA = gateway.closeWorkspace({.session = openedA.value()});
    QVERIFY(closedA);
    const auto openedB = gateway.openWorkspace({.location = location(workspaceB)});
    QVERIFY(openedB);
    const auto detailsB = gateway.readSubPrepSelectedDetails(classId(1));
    QVERIFY(detailsB);
    QCOMPARE(detailsB.value().classNotes, std::string("Workspace B class notes"));
    QCOMPARE(detailsB.value().teacherDisplayName,
             std::string("Workspace B Teacher"));
}

void WorkspaceStoreSubPrepGatewayTests::
unopenedClosedInvalidScopeAndRepositoryFailuresAreReported()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("failure.tps"));
    QString fixtureError;
    QVERIFY2(seedWorkspace(path, QStringLiteral("Failure"), &fixtureError),
             qPrintable(fixtureError));

    Platform::WorkspaceStoreGateway gateway;
    const auto unopened = gateway.readSubPrepSummaries(scopeFor({classId(1)}));
    QVERIFY(!unopened);
    QCOMPARE(unopened.error().code, Domain::ErrorCode::NotFound);
    const auto unopenedDetails = gateway.readSubPrepSelectedDetails(classId(1));
    QVERIFY(!unopenedDetails);
    QCOMPARE(unopenedDetails.error().code, Domain::ErrorCode::Technical);

    auto invalidScope = scopeFor({classId(1), classId(1)});
    const auto invalid = gateway.readSubPrepSummaries(invalidScope);
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, Domain::ErrorCode::InvalidInput);

    const auto opened = gateway.openWorkspace({.location = location(path)});
    QVERIFY(opened);
    const auto closed = gateway.closeWorkspace({.session = opened.value()});
    QVERIFY(closed);
    const auto afterClose = gateway.readSubPrepSummaries(scopeFor({classId(1)}));
    QVERIFY(!afterClose);
    QCOMPARE(afterClose.error().code, Domain::ErrorCode::NotFound);
    const auto detailsAfterClose = gateway.readSubPrepSelectedDetails(classId(1));
    QVERIFY(!detailsAfterClose);
    QCOMPARE(detailsAfterClose.error().code, Domain::ErrorCode::Technical);

    const auto reopened = gateway.openWorkspace({.location = location(path)});
    QVERIFY(reopened);
    QVERIFY(executeOnSeparateConnection(
        path,
        QStringLiteral("PRAGMA foreign_keys=OFF")));
    QVERIFY(executeOnSeparateConnection(
        path,
        QStringLiteral("ALTER TABLE class_info RENAME TO class_info_unavailable")));
    const auto repositoryFailure = gateway.readSubPrepSummaries(
        scopeFor({classId(1)}));
    QVERIFY(!repositoryFailure);
    QVERIFY(repositoryFailure.error().code == Domain::ErrorCode::Technical
        || repositoryFailure.error().code == Domain::ErrorCode::NotFound);
}

QTEST_MAIN(WorkspaceStoreSubPrepGatewayTests)
#include "workspace_store_sub_prep_gateway_tests.moc"

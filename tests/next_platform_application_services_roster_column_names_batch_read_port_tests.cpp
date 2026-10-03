#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "next/application/roster_column_names_batch_read_query.h"
#include "next/platform/application_services_roster_column_names_batch_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("roster-column-names-batch-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId typedClassId(const int classId)
{
    return *Domain::ClassId::fromString(std::to_string(classId));
}

std::vector<Domain::ClassId> typedClassIds(const QList<int>& classIds)
{
    std::vector<Domain::ClassId> result;
    result.reserve(static_cast<std::size_t>(classIds.size()));
    for (const int classId : classIds)
    {
        result.push_back(typedClassId(classId));
    }
    return result;
}

bool insertColumns(
    QSqlDatabase database,
    const int classId,
    const QList<QPair<QString, int>>& columns
    )
{
    QSqlQuery insert(database);
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }

    for (const auto& column : columns)
    {
        insert.bindValue(0, classId);
        insert.bindValue(1, column.first);
        insert.bindValue(2, column.second);
        insert.bindValue(3, 100 + column.second);
        if (!insert.exec())
        {
            return false;
        }
    }
    return true;
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.classService()->create(name);
    return created.value_or(-1);
}

}

class NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void mapsOneOrderedBatchOfFourClassesAndReadsNamesOnly();
    void returnsSuccessfulEmptySlotsForMissingRosters();
    void doesNotApplyOutputColumnLimitToDiscovery();
    void emptyAndInvalidRequestsAvoidSessionAccess();
    void rosterColumnsStatementFailureIsStructured();
};

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
mapsOneOrderedBatchOfFourClassesAndReadsNamesOnly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int zuluId = createClass(services, QStringLiteral("Zulu"));
    const int alphaId = createClass(services, QStringLiteral("Alpha"));
    const int betaId = createClass(services, QStringLiteral("Beta"));
    const int emptyId = createClass(services, QStringLiteral("Empty"));
    QVERIFY(zuluId > 0);
    QVERIFY(alphaId > 0);
    QVERIFY(betaId > 0);
    QVERIFY(emptyId > 0);

    QSqlDatabase database = services.databaseSession()->database();
    QVERIFY(insertColumns(database, zuluId, {
        {QStringLiteral("Last"), 2},
        {QStringLiteral("First"), 0},
        {QStringLiteral("Middle"), 1}
    }));
    QVERIFY(insertColumns(database, alphaId, {
        {QStringLiteral("Korean"), 0},
        {QStringLiteral("Memo Books"), 1}
    }));
    QVERIFY(insertColumns(database, betaId, {
        {QStringLiteral("Beta Note"), 0}
    }));

    // Removing roster_data proves this discovery path does not materialize
    // the full roster snapshot or its student rows.
    QSqlQuery dropRosterData(database);
    QVERIFY2(dropRosterData.exec(QStringLiteral("DROP TABLE roster_data")),
        qPrintable(dropRosterData.lastError().text()));

    const QList<int> orderedIds{zuluId, emptyId, alphaId, betaId};
    const auto ids = typedClassIds(orderedIds);
    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const Application::RosterColumnNamesBatchReadQuery query(port);
    const auto result = query.execute(ids);

    QVERIFY2(result, result ? "" : qPrintable(
        QString::fromStdString(result.error().message)));
    QCOMPARE(result.value().size(), std::size_t(4));
    QCOMPARE(result.value().at(0).classId.value(), std::to_string(zuluId));
    QVERIFY(result.value().at(0).columns ==
        (std::vector<std::u16string>{u"First", u"Middle", u"Last"}));
    QCOMPARE(result.value().at(1).classId.value(), std::to_string(emptyId));
    QVERIFY(result.value().at(1).columns.empty());
    QCOMPARE(result.value().at(2).classId.value(), std::to_string(alphaId));
    QVERIFY(result.value().at(2).columns ==
        (std::vector<std::u16string>{u"Korean", u"Memo Books"}));
    QCOMPARE(result.value().at(3).classId.value(), std::to_string(betaId));
    QVERIFY(result.value().at(3).columns ==
        (std::vector<std::u16string>{u"Beta Note"}));
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
returnsSuccessfulEmptySlotsForMissingRosters()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int withRosterId = createClass(
        services,
        QStringLiteral("With Roster")
        );
    const int withoutRosterId = createClass(
        services,
        QStringLiteral("Without Roster")
        );
    QVERIFY(withRosterId > 0);
    QVERIFY(withoutRosterId > 0);
    QVERIFY(insertColumns(
        services.databaseSession()->database(),
        withRosterId,
        {{QStringLiteral("Notes"), 0}}
        ));

    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const auto result = port.readRosterColumnNames(
        typedClassIds({withoutRosterId, withRosterId})
        );

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value().at(0).classId.value(), std::to_string(withoutRosterId));
    QVERIFY(result.value().at(0).columns.empty());
    QCOMPARE(result.value().at(1).classId.value(), std::to_string(withRosterId));
    QVERIFY(result.value().at(1).columns == std::vector<std::u16string>{u"Notes"});
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
doesNotApplyOutputColumnLimitToDiscovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Roster With Many Columns")
        );
    QVERIFY(classId > 0);

    QList<QPair<QString, int>> columns;
    columns.reserve(129);
    for (int index = 0; index < 129; ++index)
    {
        columns.append(qMakePair(
            QStringLiteral("Column %1").arg(index),
            index
            ));
    }
    QVERIFY(insertColumns(
        services.databaseSession()->database(),
        classId,
        columns
        ));

    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const auto result = port.readRosterColumnNames({typedClassId(classId)});

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(1));
    QCOMPARE(result.value().at(0).columns.size(), std::size_t(129));
    QCOMPARE(result.value().at(0).columns.front(), std::u16string(u"Column 0"));
    QCOMPARE(result.value().at(0).columns.back(), std::u16string(u"Column 128"));
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
emptyAndInvalidRequestsAvoidSessionAccess()
{
    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(
        static_cast<ApplicationServices*>(nullptr)
        );

    const auto empty = port.readRosterColumnNames({});
    QVERIFY(empty);
    QVERIFY(empty.value().empty());

    const auto invalid = port.readRosterColumnNames({
        *Domain::ClassId::fromString("01")
    });
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, Domain::ErrorCode::InvalidInput);
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
rosterColumnsStatementFailureIsStructured()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services, QStringLiteral("Broken Roster"));
    QVERIFY(classId > 0);

    QSqlQuery dropRosterColumns(services.databaseSession()->database());
    QVERIFY2(dropRosterColumns.exec(QStringLiteral("DROP TABLE roster_columns")),
        qPrintable(dropRosterColumns.lastError().text()));

    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const Application::RosterColumnNamesBatchReadQuery query(port);
    const auto result = query.execute({typedClassId(classId)});

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

QTEST_MAIN(NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests)

#include "next_platform_application_services_roster_column_names_batch_read_port_tests.moc"

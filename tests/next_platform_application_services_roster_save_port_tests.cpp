#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "next/application/roster_save_use_case.h"
#include "next/platform/application_services_roster_save_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("roster-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Application::RosterSaveRequest request(
    const int classId,
    const bool allowQuestionableKoreanNameLengths = false
    )
{
    Application::RosterSnapshot snapshot;
    snapshot.columns = {
        u"English",
        u"Korean",
        u"Winter",
        u"Speech Contest",
        u"Summer",
        u"Fall",
        u"備考"
    };
    snapshot.columnWidths = {172, 123, 131, 132, 133, 134, 207};
    snapshot.rows.resize(25);
    for (int row = 0; row < 25; ++row)
    {
        std::u16string english = u"Student ";
        english.push_back(static_cast<char16_t>(u'A' + row));

        snapshot.rows[static_cast<std::size_t>(row)] = {
            std::move(english),
            u"김민지",
            row % 2 == 0 ? u"A+" : u"B",
            u"B+",
            u"A",
            u"B",
            row == 0 ? u"수업 \U0001F4DA" : u""
        };
    }

    const auto classIdentifier = Domain::ClassId::fromString(
        std::to_string(classId)
        );
    if (!classIdentifier)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {
        .classId = *classIdentifier,
        .roster = std::move(snapshot),
        .allowQuestionableKoreanNameLengths = allowQuestionableKoreanNameLengths
    };
}

}

class NextPlatformApplicationServicesRosterSavePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsNonCanonicalIdsBeforeSessionAccess();
    void savesCompleteRosterSnapshotToOpenSession();
    void forwardsQuestionableKoreanNameDecision();
    void repositoryFailureMapsTechnicalAndRollsBack();
    void closedSessionReturnsFailureWithoutDataServiceFallback();
};

void NextPlatformApplicationServicesRosterSavePortTests::
rejectsNonCanonicalIdsBeforeSessionAccess()
{
    ApplicationServices services;
    Platform::ApplicationServicesRosterSavePort port(services);

    for (const std::string classId : {
             "0",
             "-2",
             "+42",
             "class-42",
             " 42",
             "42 ",
             "01",
             "00042",
             "999999999999999999999"
         })
    {
        const auto typedId = Domain::ClassId::fromString(classId);
        QVERIFY(typedId);

        Application::RosterSaveRequest saveRequest = request(1);
        saveRequest.classId = *typedId;
        const auto result = port.saveRoster(saveRequest);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
}

void NextPlatformApplicationServicesRosterSavePortTests::
savesCompleteRosterSnapshotToOpenSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Roster Save Test")
        );
    QVERIFY(createdClass);

    Application::RosterSaveRequest saveRequest = request(*createdClass);
    saveRequest.roster.columns[0] = u" English ";
    saveRequest.roster.columns[3] = u" Speech   Contest ";
    saveRequest.roster.rows[0][0] = u"  Student   A  ";
    Platform::ApplicationServicesRosterSavePort port(services);
    const auto saved =
        Application::RosterSaveUseCase::execute(saveRequest, port);
    QVERIFY2(
        saved,
        qPrintable(saved ? QString() : QString::fromStdString(saved.error().message))
        );

    RosterRepository* const repository =
        services.databaseSession()->rosterRepository();
    QVERIFY(repository);
    const auto persisted = repository->loadRoster(*createdClass);
    QVERIFY(persisted);
    QCOMPARE(persisted->columns,
        QStringList({
            "English", "Korean", "Winter", "Speech Contest",
            "Summer", "Fall", "備考"
        }));
    QCOMPARE(persisted->columnWidths,
        QVector<int>({172, 123, 131, 132, 133, 134, 207}));
    QCOMPARE(persisted->rows.size(), 25);
    QCOMPARE(persisted->rows.at(0).at(0), QStringLiteral("Student A"));
    QCOMPARE(persisted->rows.at(0).at(6), QStringLiteral("수업 \U0001F4DA"));
    QCOMPARE(persisted->rows.at(24).at(0), QStringLiteral("Student Y"));
    QCOMPARE(persisted->rows.at(24).at(2), QStringLiteral("A+"));
}

void NextPlatformApplicationServicesRosterSavePortTests::
forwardsQuestionableKoreanNameDecision()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Roster Save Validation Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesRosterSavePort port(services);
    const auto originalSaved = Application::RosterSaveUseCase::execute(
        request(*createdClass),
        port
        );
    QVERIFY2(
        originalSaved,
        qPrintable(
            originalSaved
                ? QString()
                : QString::fromStdString(originalSaved.error().message)
            )
        );

    Application::RosterSaveRequest saveRequest = request(*createdClass);
    saveRequest.roster.rows[0][1] = u"김";
    for (std::size_t row = 1; row < saveRequest.roster.rows.size(); ++row)
    {
        saveRequest.roster.rows[row] = {};
    }

    const auto rejected =
        Application::RosterSaveUseCase::execute(saveRequest, port);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!rejected.error().message.empty());

    RosterRepository* const repository =
        services.databaseSession()->rosterRepository();
    QVERIFY(repository);
    const auto beforeRejected = repository->loadRoster(*createdClass);
    QVERIFY(beforeRejected);
    const auto unchanged = repository->loadRoster(*createdClass);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->columns, beforeRejected->columns);
    QCOMPARE(unchanged->columnWidths, beforeRejected->columnWidths);
    QCOMPARE(unchanged->rows, beforeRejected->rows);

    saveRequest.allowQuestionableKoreanNameLengths = true;
    const auto accepted =
        Application::RosterSaveUseCase::execute(saveRequest, port);
    QVERIFY2(
        accepted,
        qPrintable(
            accepted ? QString() : QString::fromStdString(accepted.error().message)
            )
        );

    const auto persisted = repository->loadRoster(*createdClass);
    QVERIFY(persisted);
    QCOMPARE(persisted->rows.size(), 1);
    QCOMPARE(persisted->rows.at(0).at(1), QStringLiteral("김"));
}

void NextPlatformApplicationServicesRosterSavePortTests::
repositoryFailureMapsTechnicalAndRollsBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Roster Save Repository Failure Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesRosterSavePort port(services);
    const Application::RosterSaveRequest original = request(*createdClass);
    const auto originalSaved =
        Application::RosterSaveUseCase::execute(original, port);
    QVERIFY2(
        originalSaved,
        qPrintable(
            originalSaved
                ? QString()
                : QString::fromStdString(originalSaved.error().message)
            )
        );

    QSqlQuery trigger(services.databaseSession()->database());
    QVERIFY2(
        trigger.exec(QStringLiteral(
            "CREATE TRIGGER reject_roster_data_insert "
            "BEFORE INSERT ON roster_data "
            "WHEN NEW.value = 'Injected Failure' "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected roster failure'); "
            "END"
            )),
        qPrintable(trigger.lastError().text())
        );

    Application::RosterSaveRequest changed = original;
    changed.roster.rows[0][0] = u"Injected Failure";
    const auto failed =
        Application::RosterSaveUseCase::execute(changed, port);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());

    RosterRepository* const repository =
        services.databaseSession()->rosterRepository();
    QVERIFY(repository);
    const auto persisted = repository->loadRoster(*createdClass);
    QVERIFY(persisted);
    QCOMPARE(persisted->columns.size(), 7);
    QCOMPARE(persisted->columnWidths.size(), 7);
    QCOMPARE(persisted->rows.size(), 25);
    QCOMPARE(persisted->rows.at(0).at(0), QStringLiteral("Student A"));
    QCOMPARE(persisted->rows.at(24).at(0), QStringLiteral("Student Y"));
}

void NextPlatformApplicationServicesRosterSavePortTests::
closedSessionReturnsFailureWithoutDataServiceFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Closed Roster Save Test")
        );
    QVERIFY(createdClass);

    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.dataService());
    QVERIFY(!services.dataService()->isOpen());

    Platform::ApplicationServicesRosterSavePort port(services);
    const auto result = Application::RosterSaveUseCase::execute(
        request(*createdClass),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

QTEST_MAIN(NextPlatformApplicationServicesRosterSavePortTests)

#include "next_platform_application_services_roster_save_port_tests.moc"

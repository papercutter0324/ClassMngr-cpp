#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "next/platform/application_services_schedule_slot_state_save_port.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <algorithm>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("schedule-slot-state-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Result<QList<IntensiveSlotState>> loadSlotStates(
    ApplicationServices& services
    )
{
    return services.workspaceStore()
        ->intensiveSlotStateRepository()
        ->loadIntensiveSlotStates();
}

Application::ScheduleSlotStateSaveRequest request(
    const Application::ScheduleSlotState selectedState
    )
{
    return {
        .weekday = Application::ScheduleWeekday::Thursday,
        .startMinute = Application::ScheduleSlotStartMinute(9 * 60 + 5),
        .selectedState = selectedState,
        .defaultState = Application::ScheduleSlotState::Essay
    };
}

}

class NextPlatformApplicationServicesScheduleSlotStateSavePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void savesNondefaultStateUsingTypedDayAndMinute();
    void savesMinuteOfDayBoundaries();
    void selectingDefaultStateDeletesStoredOverride();
    void invalidInputDoesNotWrite();
    void repositoryWriteFailureIsTechnicalAndLeavesNoOverride();
    void unavailableAndClosedSessionsDoNotWriteThroughFallback();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
savesNondefaultStateUsingTypedDayAndMinute()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));

    Platform::ApplicationServicesScheduleSlotStateSavePort port(services);
    QVERIFY(port.isAvailable());
    const auto saved = port.saveSlotState(
        request(Application::ScheduleSlotState::Lunch)
        );
    QVERIFY(saved);

    const auto loaded = loadSlotStates(services);
    QVERIFY(loaded);
    QCOMPARE(loaded->size(), 1);
    QCOMPARE(loaded->first().day, QStringLiteral("Thursday"));
    QCOMPARE(loaded->first().startTime, QStringLiteral("09:05"));
    QCOMPARE(loaded->first().state, QStringLiteral("lunch"));
}

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
savesMinuteOfDayBoundaries()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));

    Platform::ApplicationServicesScheduleSlotStateSavePort port(services);
    auto midnight = request(Application::ScheduleSlotState::Lunch);
    midnight.weekday = Application::ScheduleWeekday::Monday;
    midnight.startMinute = Application::ScheduleSlotStartMinute(0);
    QVERIFY(port.saveSlotState(midnight));

    auto lastMinute = request(Application::ScheduleSlotState::Lunch);
    lastMinute.weekday = Application::ScheduleWeekday::Friday;
    lastMinute.startMinute = Application::ScheduleSlotStartMinute(24 * 60 - 1);
    QVERIFY(port.saveSlotState(lastMinute));

    const auto loaded = loadSlotStates(services);
    QVERIFY(loaded);
    QCOMPARE(loaded->size(), 2);
    QVERIFY(std::any_of(
        loaded->cbegin(),
        loaded->cend(),
        [](const IntensiveSlotState& state)
        {
            return state.day == QStringLiteral("Monday")
                && state.startTime == QStringLiteral("00:00")
                && state.state == QStringLiteral("lunch");
        }
        ));
    QVERIFY(std::any_of(
        loaded->cbegin(),
        loaded->cend(),
        [](const IntensiveSlotState& state)
        {
            return state.day == QStringLiteral("Friday")
                && state.startTime == QStringLiteral("23:59")
                && state.state == QStringLiteral("lunch");
        }
        ));
}

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
selectingDefaultStateDeletesStoredOverride()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));

    Platform::ApplicationServicesScheduleSlotStateSavePort port(services);
    IntensiveSlotStateRepository* const repository =
        services.workspaceStore()->intensiveSlotStateRepository();
    QVERIFY(repository->saveIntensiveSlotState(
        QStringLiteral("Monday"),
        QStringLiteral("09:00"),
        QStringLiteral("lunch")
        ));
    QVERIFY(port.saveSlotState(
        request(Application::ScheduleSlotState::Lunch)
        ));

    const auto deleted = port.saveSlotState(
        request(Application::ScheduleSlotState::Essay)
        );
    QVERIFY(deleted);

    const auto loaded = loadSlotStates(services);
    QVERIFY(loaded);
    QCOMPARE(loaded->size(), 1);
    QCOMPARE(loaded->first().day, QStringLiteral("Monday"));
    QCOMPARE(loaded->first().startTime, QStringLiteral("09:00"));
    QCOMPARE(loaded->first().state, QStringLiteral("lunch"));
}

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
invalidInputDoesNotWrite()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));

    Platform::ApplicationServicesScheduleSlotStateSavePort port(services);
    const int invalidMinutes[] = {-1, 24 * 60};
    for (const int minute : invalidMinutes)
    {
        auto invalid = request(Application::ScheduleSlotState::Lunch);
        invalid.startMinute = Application::ScheduleSlotStartMinute(minute);
        const auto result = port.saveSlotState(invalid);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("Schedule slot start minute must be within the day.")
            );
        QVERIFY(!result.error().recoverable);
    }

    const auto loaded = loadSlotStates(services);
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
repositoryWriteFailureIsTechnicalAndLeavesNoOverride()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));
    QSqlQuery query(services.workspaceStore()->database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_slot_state_insert "
        "BEFORE INSERT ON intensive_slot_states "
        "BEGIN SELECT RAISE(ABORT, 'injected slot-state failure'); END"
        )));

    Platform::ApplicationServicesScheduleSlotStateSavePort port(services);
    const auto result = port.saveSlotState(
        request(Application::ScheduleSlotState::Lunch)
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(!result.error().message.empty());

    const auto loaded = loadSlotStates(services);
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
unavailableAndClosedSessionsDoNotWriteThroughFallback()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    Platform::ApplicationServicesScheduleSlotStateSavePort port(services);
    QVERIFY(!port.isAvailable());
    const auto unavailable = port.saveSlotState(
        request(Application::ScheduleSlotState::Lunch)
        );
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);

    QVERIFY(services.openDatabase(path));
    QVERIFY(port.isAvailable());
    services.closeDatabase();
    QVERIFY(!port.isAvailable());

    const auto saved = port.saveSlotState(
        request(Application::ScheduleSlotState::Lunch)
        );
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::NotFound);

    QVERIFY(services.openDatabase(path));
    const auto loaded = loadSlotStates(services);
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

QTEST_MAIN(NextPlatformApplicationServicesScheduleSlotStateSavePortTests)

#include "next_platform_application_services_schedule_slot_state_save_port_tests.moc"

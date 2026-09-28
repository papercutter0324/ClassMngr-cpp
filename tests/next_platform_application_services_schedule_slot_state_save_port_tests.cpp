#include "core/application_services.h"
#include "next/platform/application_services_schedule_slot_state_save_port.h"

#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

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

Application::ScheduleSlotStateSaveRequest request(
    const Application::ScheduleSlotState selectedState
    )
{
    return {
        .weekday = Application::ScheduleWeekday::Thursday,
        .startMinute = 10 * 60 + 5,
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
    void selectingDefaultStateDeletesStoredOverride();
    void closedSessionDoesNotWriteThroughDataServiceFallback();

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

    const auto loaded = services.scheduleService()->intensiveSlotStates();
    QVERIFY(loaded);
    QCOMPARE(loaded->size(), 1);
    QCOMPARE(loaded->first().day, QStringLiteral("Thursday"));
    QCOMPARE(loaded->first().startTime, QStringLiteral("10:05"));
    QCOMPARE(loaded->first().state, QStringLiteral("lunch"));
}

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
selectingDefaultStateDeletesStoredOverride()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));

    Platform::ApplicationServicesScheduleSlotStateSavePort port(services);
    QVERIFY(port.saveSlotState(
        request(Application::ScheduleSlotState::Lunch)
        ));

    const auto deleted = port.saveSlotState(
        request(Application::ScheduleSlotState::Essay)
        );
    QVERIFY(deleted);

    const auto loaded = services.scheduleService()->intensiveSlotStates();
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

void NextPlatformApplicationServicesScheduleSlotStateSavePortTests::
closedSessionDoesNotWriteThroughDataServiceFallback()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));

    Platform::ApplicationServicesScheduleSlotStateSavePort port(services);
    QVERIFY(port.isAvailable());
    services.closeDatabase();
    QVERIFY(!port.isAvailable());

    const auto saved = port.saveSlotState(
        request(Application::ScheduleSlotState::Lunch)
        );
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::NotFound);

    QVERIFY(services.openDatabase(path));
    const auto loaded = services.scheduleService()->intensiveSlotStates();
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

QTEST_MAIN(NextPlatformApplicationServicesScheduleSlotStateSavePortTests)

#include "next_platform_application_services_schedule_slot_state_save_port_tests.moc"

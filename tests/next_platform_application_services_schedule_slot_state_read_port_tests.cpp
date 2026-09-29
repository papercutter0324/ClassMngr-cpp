#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/schedule_slot_state_read_query.h"
#include "next/platform/application_services_schedule_slot_state_read_port.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory, const QString& prefix)
{
    return directory.filePath(
        QStringLiteral("%1-%2.tps").arg(
            prefix,
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool insertSlotState(
    QSqlDatabase database,
    const QString& day,
    const QString& startTime,
    const QString& state
    )
{
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO intensive_slot_states (day, start_time, state) "
        "VALUES (?, ?, ?)"
        ));
    insert.addBindValue(day);
    insert.addBindValue(startTime);
    insert.addBindValue(state);
    return insert.exec();
}

}

class NextPlatformApplicationServicesScheduleSlotStateReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsOrderedRawRowsFromTheActiveSession();
    void returnsEmptySuccessWhenNoOverridesExist();
    void distinguishesUnavailableSessionFromRepositoryFailure();
};

void NextPlatformApplicationServicesScheduleSlotStateReadPortTests::
readsOrderedRawRowsFromTheActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(
        directory,
        QStringLiteral("active")
        )));
    const QSqlDatabase database = services.databaseSession()->database();
    QVERIFY(insertSlotState(
        database,
        QStringLiteral("Tuesday "),
        QStringLiteral("09:05 "),
        QString::fromUcs4(U" mystery 🧭 ")
        ));
    QVERIFY(insertSlotState(
        database,
        QStringLiteral("Monday"),
        QStringLiteral("10:00"),
        QStringLiteral("lunch")
        ));
    QVERIFY(insertSlotState(
        database,
        QStringLiteral("Monday"),
        QStringLiteral("09:00"),
        QStringLiteral("unknown state")
        ));

    Platform::ApplicationServicesScheduleSlotStateReadPort port(services);
    const auto result = Application::ScheduleSlotStateReadQueryHandler::execute(
        {},
        port
        );

    QVERIFY(result);
    QCOMPARE(result.value().rows.size(), std::size_t(3));
    QCOMPARE(result.value().rows[0], (Application::ScheduleSlotStateReadRow{
        u"Monday", u"09:00", u"unknown state"
    }));
    QCOMPARE(result.value().rows[1], (Application::ScheduleSlotStateReadRow{
        u"Monday", u"10:00", u"lunch"
    }));
    QCOMPARE(result.value().rows[2], (Application::ScheduleSlotStateReadRow{
        u"Tuesday ", u"09:05 ", u" mystery 🧭 "
    }));
}

void NextPlatformApplicationServicesScheduleSlotStateReadPortTests::
returnsEmptySuccessWhenNoOverridesExist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(
        directory,
        QStringLiteral("empty")
        )));

    Platform::ApplicationServicesScheduleSlotStateReadPort port(services);
    const auto result = Application::ScheduleSlotStateReadQueryHandler::execute(
        {},
        port
        );

    QVERIFY(result);
    QVERIFY(result.value().rows.empty());
}

void NextPlatformApplicationServicesScheduleSlotStateReadPortTests::
distinguishesUnavailableSessionFromRepositoryFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString legacyPath = databasePath(
        directory,
        QStringLiteral("legacy")
        );
    DataService legacyData(legacyPath);
    QVERIFY(legacyData.open());
    QVERIFY(legacyData.isOpen());
    QVERIFY(legacyData.saveIntensiveSlotState(
        QStringLiteral("Friday"),
        QStringLiteral("09:00"),
        QStringLiteral("lunch")
        ));
    ScheduleService legacyScheduleService(&legacyData);
    QVERIFY(legacyScheduleService.isAvailable());
    const auto legacyRows = legacyScheduleService.intensiveSlotStates();
    QVERIFY(legacyRows);
    QCOMPARE(legacyRows->size(), 1);

    ApplicationServices unavailableServices;
    Platform::ApplicationServicesScheduleSlotStateReadPort unavailablePort(
        unavailableServices
        );
    auto result = Application::ScheduleSlotStateReadQueryHandler::execute(
        {},
        unavailablePort
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices failedReadServices;
    QVERIFY(failedReadServices.openDatabase(databasePath(
        directory,
        QStringLiteral("failed-read")
        )));
    QSqlQuery dropSlotStateTable(
        failedReadServices.databaseSession()->database()
        );
    QVERIFY(dropSlotStateTable.exec(QStringLiteral(
        "DROP TABLE intensive_slot_states"
        )));

    Platform::ApplicationServicesScheduleSlotStateReadPort failedReadPort(
        failedReadServices
        );
    result = Application::ScheduleSlotStateReadQueryHandler::execute(
        {},
        failedReadPort
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(result.error().message.find(
        "Loading intensive slot states"
        ) != std::string::npos);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesScheduleSlotStateReadPortTests
    )

#include "next_platform_application_services_schedule_slot_state_read_port_tests.moc"

#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"

#include <QRegularExpression>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto AcademicCalendarScheduleKey =
    "calendar/academicSchedule/v1";
constexpr auto UnrelatedKey = "calendar/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("academic-calendar-schedule-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
{
    return services.openDatabase(databasePath(directory)).has_value();
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
    return query.exec(statement);
}

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

} // namespace

class NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingAndUnavailableReadEmptyAndIgnoreSave();
    void readErrorsReturnEmpty();
    void roundTripsThroughTheExactKey();
    void preservesStoredPayloadAndUnrelatedSettings();
    void saveFailurePreservesWarningAndStoredPayload();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
missingAndUnavailableReadEmptyAndIgnoreSave()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    QVERIFY(port.read().empty());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    DatabaseSession* const unavailableSession =
        unavailableServices.databaseSession();
    QVERIFY(unavailableSession);
    QVERIFY(!unavailableSession->isOpen());
    ApplicationServicesAcademicCalendarSchedulePreferencesPort unavailablePort(
        unavailableServices
        );
    QVERIFY(unavailablePort.read().empty());
    unavailablePort.write(R"({"version":1})");
    QVERIFY(unavailablePort.read().empty());

    ApplicationServices closedServices;
    const QString closedDatabasePath = databasePath(m_directory);
    QVERIFY(closedServices.openDatabase(closedDatabasePath));
    QVERIFY(closedServices.dataService());
    DatabaseSession* const closedSession = closedServices.databaseSession();
    QVERIFY(closedSession);
    QVERIFY(closedSession->isOpen());
    SettingsRepository* const closedRepository =
        closedSession->settingsRepository();
    QVERIFY(closedRepository);
    const QString oldPayload = QStringLiteral(
        R"({"version":1,"profiles":{"old":true}})"
        );
    QVERIFY(closedRepository->saveSetting(
        QString::fromUtf8(AcademicCalendarScheduleKey),
        oldPayload
        ));

    ApplicationServicesAcademicCalendarSchedulePreferencesPort closedPort(
        closedServices
        );
    closedServices.closeDatabase();
    QVERIFY(closedServices.dataService());
    QVERIFY(!closedSession->isOpen());
    QVERIFY(closedPort.read().empty());
    closedPort.write(R"({"version":1,"profiles":{"new":true}})");
    QVERIFY(closedPort.read().empty());

    QVERIFY(closedServices.openDatabase(closedDatabasePath));
    QVERIFY(closedSession->isOpen());
    SettingsRepository* const reopenedRepository =
        closedSession->settingsRepository();
    QVERIFY(reopenedRepository);
    const auto unchanged = reopenedRepository->loadSetting(
        QString::fromUtf8(AcademicCalendarScheduleKey)
        );
    QVERIFY(unchanged);
    QCOMPARE(unchanged->toString(), oldPayload);

    ApplicationServicesAcademicCalendarSchedulePreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QVERIFY(nullPort.read().empty());
    nullPort.write(R"({"version":1})");
    QVERIFY(nullPort.read().empty());
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
readErrorsReturnEmpty()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    QVERIFY(port.read().empty());
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
roundTripsThroughTheExactKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const std::string expected =
        R"({"version":1,"profiles":{"elementary":{},"middle":{}}})";
    ApplicationServices* servicesPointer = &services;
    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(
        servicesPointer
        );
    port.write(expected);

    QCOMPARE(port.read(), expected);

    const auto stored = repository->loadSetting(
        QString::fromUtf8(AcademicCalendarScheduleKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), fromUtf8(expected));
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
preservesStoredPayloadAndUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const QString storedPayload = QString::fromUtf8(
        "{\"version\":1,\"profiles\":{\"elementary\":{\"2026\":{\"termYear\":2026}},\"middle\":{}}}"
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(AcademicCalendarScheduleKey),
            storedPayload
            )
        );
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    QCOMPARE(fromUtf8(port.read()), storedPayload);

    const std::string replacement =
        R"({"version":1,"profiles":{"replacement":true}})";
    port.write(replacement);
    QCOMPARE(port.read(), replacement);

    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests::
saveFailurePreservesWarningAndStoredPayload()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    const std::string initial = R"({"version":1,"profiles":{}})";
    const std::string replacement = R"({"version":1,"profiles":{"changed":true}})";
    ApplicationServicesAcademicCalendarSchedulePreferencesPort port(services);
    port.write(initial);
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_academic_calendar_schedule
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'calendar/academicSchedule/v1'
            BEGIN
                SELECT RAISE(ABORT, 'forced academic calendar schedule failure');
            END
        )")
        ));

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral(
                "Failed to save academic calendar schedule:.*"
                )
            )
        );
    port.write(replacement);

    QCOMPARE(port.read(), initial);
}

QTEST_MAIN(
    NextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests
    )

#include "next_platform_application_services_academic_calendar_schedule_preferences_port_tests.moc"

#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "domain/models/calendar_event.h"
#include "next/platform/application_services_calendar_event_type_color_preferences_port.h"

#include <QColor>
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

constexpr auto VacationKey =
    "calendar/eventTypeColor/Vacation";
constexpr auto HolidayKey =
    "calendar/eventTypeColor/Holiday";
constexpr auto OtherKey =
    "calendar/eventTypeColor/Other";
constexpr auto UnrelatedKey = "calendar/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("calendar-event-type-color-%1.tps").arg(
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

std::string utf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

} // namespace

class NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void exactDynamicKeysUseNormalizedEventTypes();
    void validHexRgbRoundTripsAndPreservesUnrelatedSettings();
    void missingAndUnavailableReadEmptyAndIgnoreSave();
    void readErrorsDefaultEmpty();
    void pointerAndNullServiceAccessPreserveFallbacks();
    void invalidStoredColorPassesThroughUnchanged();
    void saveFailurePreservesWarningAndStoredColor();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
exactDynamicKeysUseNormalizedEventTypes()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const QString rawType = QStringLiteral("  Vacation ");
    const QString normalizedType = normalizedCalendarEventType(rawType);
    QCOMPARE(normalizedType, QStringLiteral("Vacation"));

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    port.write(utf8(normalizedType), utf8(QStringLiteral("#123456")));

    QCOMPARE(port.read(utf8(normalizedType)), utf8(QStringLiteral("#123456")));

    const auto stored = repository->loadSetting(
        QString::fromUtf8(VacationKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), QStringLiteral("#123456"));

    const auto nonNormalizedKey = repository->loadSetting(
        QStringLiteral("calendar/eventTypeColor/  Vacation ")
        );
    QVERIFY(nonNormalizedKey);
    QVERIFY(!nonNormalizedKey->isValid());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
validHexRgbRoundTripsAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    const QColor expectedColor(QStringLiteral("#4b6f91"));
    QVERIFY(expectedColor.isValid());
    const std::string expectedHex =
        utf8(expectedColor.name(QColor::HexRgb));

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    port.write(utf8(QStringLiteral("Holiday")), expectedHex);
    QCOMPARE(port.read(utf8(QStringLiteral("Holiday"))), expectedHex);

    const auto stored = repository->loadSetting(
        QString::fromUtf8(HolidayKey)
        );
    QVERIFY(stored);
    QCOMPARE(stored->toString(), expectedColor.name(QColor::HexRgb));

    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
missingAndUnavailableReadEmptyAndIgnoreSave()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const openSession = services.databaseSession();
    QVERIFY(openSession);
    QVERIFY(openSession->isOpen());
    QVERIFY(openSession->settingsRepository());

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    QVERIFY(port.read(utf8(QStringLiteral("Other"))).empty());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    DatabaseSession* const unavailableSession =
        unavailableServices.databaseSession();
    QVERIFY(unavailableSession);
    QVERIFY(!unavailableSession->isOpen());
    ApplicationServicesCalendarEventTypeColorPreferencesPort unavailablePort(
        unavailableServices
        );
    QVERIFY(unavailablePort.read(utf8(QStringLiteral("Other"))).empty());
    unavailablePort.write(
        utf8(QStringLiteral("Other")),
        utf8(QStringLiteral("#ffffff"))
        );
    QVERIFY(unavailablePort.read(utf8(QStringLiteral("Other"))).empty());

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
    QVERIFY(closedRepository->saveSetting(
        QString::fromUtf8(VacationKey),
        QStringLiteral("#123456")
        ));

    ApplicationServicesCalendarEventTypeColorPreferencesPort closedPort(
        closedServices
        );
    closedServices.closeDatabase();
    QVERIFY(closedServices.dataService());
    QVERIFY(!closedSession->isOpen());
    QVERIFY(closedPort.read(utf8(QStringLiteral("Vacation"))).empty());
    closedPort.write(
        utf8(QStringLiteral("Vacation")),
        utf8(QStringLiteral("#abcdef"))
        );
    QVERIFY(closedPort.read(utf8(QStringLiteral("Vacation"))).empty());

    QVERIFY(closedServices.openDatabase(closedDatabasePath));
    QVERIFY(closedSession->isOpen());
    SettingsRepository* const reopenedRepository =
        closedSession->settingsRepository();
    QVERIFY(reopenedRepository);
    const auto unchanged = reopenedRepository->loadSetting(
        QString::fromUtf8(VacationKey)
        );
    QVERIFY(unchanged);
    QCOMPARE(unchanged->toString(), QStringLiteral("#123456"));
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
readErrorsDefaultEmpty()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    QVERIFY(port.read(utf8(QStringLiteral("Vacation"))).empty());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
pointerAndNullServiceAccessPreserveFallbacks()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    ApplicationServices* availableServices = &services;
    ApplicationServicesCalendarEventTypeColorPreferencesPort port(
        availableServices
        );
    port.write(
        utf8(QStringLiteral("Vacation")),
        utf8(QStringLiteral("#123456"))
        );
    QCOMPARE(
        port.read(utf8(QStringLiteral("Vacation"))),
        utf8(QStringLiteral("#123456"))
        );

    ApplicationServices* noServices = nullptr;
    ApplicationServicesCalendarEventTypeColorPreferencesPort nullPort(
        noServices
    );
    QVERIFY(nullPort.read(utf8(QStringLiteral("Vacation"))).empty());
    nullPort.write(
        utf8(QStringLiteral("Vacation")),
        utf8(QStringLiteral("#abcdef"))
        );
    QVERIFY(nullPort.read(utf8(QStringLiteral("Vacation"))).empty());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
invalidStoredColorPassesThroughUnchanged()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(OtherKey),
            QStringLiteral("not-a-color")
            )
        );

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    const std::string stored = port.read(utf8(QStringLiteral("Other")));
    QCOMPARE(stored, utf8(QStringLiteral("not-a-color")));
    QVERIFY(!QColor(QString::fromUtf8(
        stored.data(),
        static_cast<qsizetype>(stored.size())
        )).isValid());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
saveFailurePreservesWarningAndStoredColor()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    port.write(utf8(QStringLiteral("Vacation")), utf8(QStringLiteral("#123456")));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_calendar_event_type_color
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'calendar/eventTypeColor/Vacation'
            BEGIN
                SELECT RAISE(ABORT, 'forced calendar event color failure');
            END
        )")
        ));

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral(
                "Failed to save calendar event type color:.*"
                )
            )
        );
    port.write(utf8(QStringLiteral("Vacation")), utf8(QStringLiteral("#abcdef")));

    QCOMPARE(
        port.read(utf8(QStringLiteral("Vacation"))),
        utf8(QStringLiteral("#123456"))
        );
}

QTEST_MAIN(
    NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests
    )

#include "next_platform_application_services_calendar_event_type_color_preferences_port_tests.moc"

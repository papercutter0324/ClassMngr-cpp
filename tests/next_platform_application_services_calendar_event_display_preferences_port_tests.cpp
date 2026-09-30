#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/calendar_event_display_preferences.h"
#include "next/platform/application_services_calendar_event_display_preferences_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto ShowAllCampusesKey =
    "calendar/showEventsAtAllCampuses";
constexpr auto HideStartOfTermEventsKey =
    "calendar/hideStartOfTermEvents";
constexpr auto UnrelatedKey = "calendar/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("calendar-event-display-%1.tps").arg(
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

} // namespace

class NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingAndUnavailableSettingsDefaultFalse();
    void readErrorsDefaultFalse();
    void exactKeysRoundTripAndPreserveUnrelatedSettings();
    void preservesLegacyQVariantBooleanCoercion();
    void atomicSaveFailureRollsBackAndPreservesUnrelatedSettings();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests::
missingAndUnavailableSettingsDefaultFalse()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    ApplicationServicesCalendarEventDisplayPreferencesPort port(services);
    const auto missing = port.load();
    QVERIFY(missing);
    QVERIFY(!missing.value().showEventsAtAllCampuses);
    QVERIFY(!missing.value().hideStartOfTermEvents);

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    DatabaseSession* const unavailableSession =
        unavailableServices.databaseSession();
    QVERIFY(unavailableSession);
    QVERIFY(!unavailableSession->isOpen());
    ApplicationServicesCalendarEventDisplayPreferencesPort unavailablePort(
        unavailableServices
        );
    const auto unavailable = unavailablePort.load();
    QVERIFY(unavailable);
    QVERIFY(!unavailable.value().showEventsAtAllCampuses);
    QVERIFY(!unavailable.value().hideStartOfTermEvents);
    QVERIFY(unavailablePort.save({
        .showEventsAtAllCampuses = true,
        .hideStartOfTermEvents = true
    }));

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
    QVERIFY(closedRepository->saveSettings({
        {QString::fromUtf8(ShowAllCampusesKey), true},
        {QString::fromUtf8(HideStartOfTermEventsKey), true}
    }));

    ApplicationServicesCalendarEventDisplayPreferencesPort closedPort(
        closedServices
        );
    closedServices.closeDatabase();
    QVERIFY(closedServices.dataService());
    QVERIFY(!closedSession->isOpen());
    const auto closed = closedPort.load();
    QVERIFY(closed);
    QCOMPARE(closed.value(), CalendarEventDisplayPreferences{});
    QVERIFY(closedPort.save({
        .showEventsAtAllCampuses = false,
        .hideStartOfTermEvents = false
    }));

    QVERIFY(closedServices.openDatabase(closedDatabasePath));
    QVERIFY(closedSession->isOpen());
    SettingsRepository* const reopenedRepository =
        closedSession->settingsRepository();
    QVERIFY(reopenedRepository);
    const auto unchangedShow = reopenedRepository->loadSetting(
        QString::fromUtf8(ShowAllCampusesKey)
        );
    const auto unchangedHide = reopenedRepository->loadSetting(
        QString::fromUtf8(HideStartOfTermEventsKey)
        );
    QVERIFY(unchangedShow);
    QVERIFY(unchangedHide);
    QCOMPARE(unchangedShow->toBool(), true);
    QCOMPARE(unchangedHide->toBool(), true);

    ApplicationServicesCalendarEventDisplayPreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullServices = nullPort.load();
    QVERIFY(nullServices);
    QCOMPARE(nullServices.value(), CalendarEventDisplayPreferences{});
    QVERIFY(nullPort.save({
        .showEventsAtAllCampuses = true,
        .hideStartOfTermEvents = false
    }));
}

void NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests::
readErrorsDefaultFalse()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesCalendarEventDisplayPreferencesPort port(services);
    const auto loaded = port.load();
    QVERIFY(loaded);
    QVERIFY(!loaded.value().showEventsAtAllCampuses);
    QVERIFY(!loaded.value().hideStartOfTermEvents);
}

void NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests::
exactKeysRoundTripAndPreserveUnrelatedSettings()
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

    const CalendarEventDisplayPreferences expected{
        .showEventsAtAllCampuses = true,
        .hideStartOfTermEvents = true
    };
    ApplicationServices* servicesPointer = &services;
    ApplicationServicesCalendarEventDisplayPreferencesPort port(
        servicesPointer
        );
    QVERIFY(port.save(expected));

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), expected);

    const auto storedShowAllCampuses = repository->loadSetting(
        QString::fromUtf8(ShowAllCampusesKey)
        );
    const auto storedHideStartOfTermEvents = repository->loadSetting(
        QString::fromUtf8(HideStartOfTermEventsKey)
        );
    QVERIFY(storedShowAllCampuses);
    QVERIFY(storedHideStartOfTermEvents);
    QCOMPARE(storedShowAllCampuses->toBool(), true);
    QCOMPARE(storedHideStartOfTermEvents->toBool(), true);

    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests::
preservesLegacyQVariantBooleanCoercion()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const std::array<std::pair<QVariant, bool>, 6> values = {{
        {QVariant(QStringLiteral(" TRUE ")), true},
        {QVariant(QStringLiteral(" 0 ")), false},
        {QVariant(1), true},
        {QVariant(0), false},
        {QVariant(true), true},
        {QVariant(false), false}
    }};
    const std::array<QString, 2> keys = {{
        QString::fromUtf8(ShowAllCampusesKey),
        QString::fromUtf8(HideStartOfTermEventsKey)
    }};

    ApplicationServicesCalendarEventDisplayPreferencesPort port(services);
    for (const QString& key : keys)
    {
        for (const auto& [value, expected] : values)
        {
            QVERIFY(repository->saveSetting(key, value));
            QVERIFY(
                repository->saveSetting(
                    key == keys.at(0) ? keys.at(1) : keys.at(0),
                    false
                    )
                );

            const auto loaded = port.load();
            QVERIFY(loaded);
            if (key == keys.at(0))
            {
                QCOMPARE(loaded.value().showEventsAtAllCampuses, expected);
                QVERIFY(!loaded.value().hideStartOfTermEvents);
            }
            else
            {
                QVERIFY(!loaded.value().showEventsAtAllCampuses);
                QCOMPARE(loaded.value().hideStartOfTermEvents, expected);
            }
        }
    }
}

void NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests::
atomicSaveFailureRollsBackAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    const CalendarEventDisplayPreferences initial{
        .showEventsAtAllCampuses = false,
        .hideStartOfTermEvents = true
    };
    const CalendarEventDisplayPreferences replacement{
        .showEventsAtAllCampuses = true,
        .hideStartOfTermEvents = false
    };

    ApplicationServicesCalendarEventDisplayPreferencesPort port(services);
    QVERIFY(port.save(initial));
    QVERIFY(
        repository->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_calendar_event_display_preferences
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'calendar/showEventsAtAllCampuses'
            BEGIN
                SELECT RAISE(ABORT, 'forced calendar preference failure');
            END
        )")
        ));

    const auto saved = port.save(replacement);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), initial);

    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests
    )

#include "next_platform_application_services_calendar_event_display_preferences_port_tests.moc"

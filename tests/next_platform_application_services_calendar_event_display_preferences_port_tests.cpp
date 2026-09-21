#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
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
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
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

    ApplicationServicesCalendarEventDisplayPreferencesPort port(services);
    const auto missing = port.load();
    QVERIFY(missing);
    QVERIFY(!missing.value().showEventsAtAllCampuses);
    QVERIFY(!missing.value().hideStartOfTermEvents);

    ApplicationServices unavailableServices;
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
}

void NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests::
exactKeysRoundTripAndPreserveUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            QStringLiteral("preserved")
            )
        );

    const CalendarEventDisplayPreferences expected{
        .showEventsAtAllCampuses = true,
        .hideStartOfTermEvents = true
    };
    ApplicationServicesCalendarEventDisplayPreferencesPort port(services);
    QVERIFY(port.save(expected));

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), expected);

    const auto storedShowAllCampuses = services.dataService()->loadSetting(
        QString::fromUtf8(ShowAllCampusesKey)
        );
    const auto storedHideStartOfTermEvents = services.dataService()->loadSetting(
        QString::fromUtf8(HideStartOfTermEventsKey)
        );
    QVERIFY(storedShowAllCampuses);
    QVERIFY(storedHideStartOfTermEvents);
    QCOMPARE(storedShowAllCampuses->toBool(), true);
    QCOMPARE(storedHideStartOfTermEvents->toBool(), true);

    const auto unrelated = services.dataService()->loadSetting(
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
            QVERIFY(services.dataService()->saveSetting(key, value));
            QVERIFY(
                services.dataService()->saveSetting(
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
        services.dataService()->saveSetting(
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

    const auto unrelated = services.dataService()->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests
    )

#include "next_platform_application_services_calendar_event_display_preferences_port_tests.moc"

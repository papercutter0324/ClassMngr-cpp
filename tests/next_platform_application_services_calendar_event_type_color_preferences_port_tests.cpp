#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "domain/models/calendar_event.h"
#include "next/platform/application_services_calendar_event_type_color_preferences_port.h"

#include <QColor>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <string>
#include <utility>

using namespace ClassMngr::Next::Platform;
using ClassMngr::Next::Domain::CalendarEventType;

namespace
{

constexpr auto VacationKey =
    "calendar/eventTypeColor/Vacation";
constexpr auto HolidayKey =
    "calendar/eventTypeColor/Holiday";
constexpr auto WorkshopKey =
    "calendar/eventTypeColor/Workshop";
constexpr auto CMKey =
    "calendar/eventTypeColor/CM";
constexpr auto MeetingKey =
    "calendar/eventTypeColor/Meeting";
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
    WorkspaceStore* const session = services.workspaceStore();
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
    void exactDynamicKeysCoverAllEventTypes();
    void validHexRgbRoundTripsAndPreservesUnrelatedSettings();
    void missingAndUnavailableReadEmptyAndIgnoreSave();
    void readErrorsDefaultEmpty();
    void pointerAndNullServiceAccessPreserveFallbacks();
    void invalidStoredColorPassesThroughUnchanged();
    void saveFailureReturnsTechnicalFailureAndPreservesStoredValues();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
exactDynamicKeysCoverAllEventTypes()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    const std::array eventTypes = {
        std::pair{CalendarEventType::Vacation, VacationKey},
        std::pair{CalendarEventType::Holiday, HolidayKey},
        std::pair{CalendarEventType::Workshop, WorkshopKey},
        std::pair{CalendarEventType::CM, CMKey},
        std::pair{CalendarEventType::Meeting, MeetingKey},
        std::pair{CalendarEventType::Other, OtherKey}
    };

    int colorSuffix = 0;
    for (const auto& [eventType, expectedKey] : eventTypes)
    {
        const QString color = QStringLiteral("#1234%1%2")
            .arg(colorSuffix / 16, 1, 16)
            .arg(colorSuffix % 16, 1, 16);
        ++colorSuffix;

        QVERIFY(port.write(eventType, utf8(color)));
        QCOMPARE(port.read(eventType), utf8(color));

        const auto stored = repository->loadSetting(
            QString::fromUtf8(expectedKey)
            );
        QVERIFY(stored);
        QCOMPARE(stored->toString(), color);
    }

    const auto invalidEventType = static_cast<CalendarEventType>(-1);
    const std::string invalidTypeColor = "#abcdef";
    QVERIFY(port.write(invalidEventType, invalidTypeColor));
    QCOMPARE(port.read(invalidEventType), invalidTypeColor);
    QCOMPARE(port.read(CalendarEventType::Other), invalidTypeColor);
    const auto invalidTypeStored = repository->loadSetting(
        QString::fromUtf8(OtherKey)
        );
    QVERIFY(invalidTypeStored);
    QCOMPARE(invalidTypeStored->toString(), QStringLiteral("#abcdef"));
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
validHexRgbRoundTripsAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
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
    QVERIFY(port.write(CalendarEventType::Holiday, expectedHex));
    QCOMPARE(port.read(CalendarEventType::Holiday), expectedHex);

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
    WorkspaceStore* const openSession = services.workspaceStore();
    QVERIFY(openSession);
    QVERIFY(openSession->isOpen());
    QVERIFY(openSession->settingsRepository());

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    QVERIFY(port.read(CalendarEventType::Other).empty());

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    WorkspaceStore* const unavailableSession =
        unavailableServices.workspaceStore();
    QVERIFY(unavailableSession);
    QVERIFY(!unavailableSession->isOpen());
    ApplicationServicesCalendarEventTypeColorPreferencesPort unavailablePort(
        unavailableServices
        );
    QVERIFY(unavailablePort.read(CalendarEventType::Other).empty());
    const auto unavailableSave = unavailablePort.write(
        CalendarEventType::Other,
        utf8(QStringLiteral("#ffffff"))
        );
    QVERIFY(!unavailableSave);
    QCOMPARE(
        unavailableSave.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(!unavailableSave.error().message.empty());
    QVERIFY(!unavailableSave.error().recoverable);
    QVERIFY(unavailablePort.read(CalendarEventType::Other).empty());

    ApplicationServices closedServices;
    const QString closedDatabasePath = databasePath(m_directory);
    QVERIFY(closedServices.openDatabase(closedDatabasePath));
    QVERIFY(closedServices.dataService());
    WorkspaceStore* const closedSession = closedServices.workspaceStore();
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
    QVERIFY(closedPort.read(CalendarEventType::Vacation).empty());
    const auto closedSave = closedPort.write(
        CalendarEventType::Vacation,
        utf8(QStringLiteral("#abcdef"))
        );
    QVERIFY(!closedSave);
    QCOMPARE(
        closedSave.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(!closedSave.error().message.empty());
    QVERIFY(!closedSave.error().recoverable);
    QVERIFY(closedPort.read(CalendarEventType::Vacation).empty());

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
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    QVERIFY(port.read(CalendarEventType::Vacation).empty());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
pointerAndNullServiceAccessPreserveFallbacks()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->settingsRepository());

    ApplicationServices* availableServices = &services;
    ApplicationServicesCalendarEventTypeColorPreferencesPort port(
        availableServices
        );
    QVERIFY(port.write(
        CalendarEventType::Vacation,
        utf8(QStringLiteral("#123456"))
        ));
    QCOMPARE(
        port.read(CalendarEventType::Vacation),
        utf8(QStringLiteral("#123456"))
        );

    ApplicationServices* noServices = nullptr;
    ApplicationServicesCalendarEventTypeColorPreferencesPort nullPort(
        noServices
    );
    QVERIFY(nullPort.read(CalendarEventType::Vacation).empty());
    const auto nullSave = nullPort.write(
        CalendarEventType::Vacation,
        utf8(QStringLiteral("#abcdef"))
        );
    QVERIFY(!nullSave);
    QCOMPARE(
        nullSave.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(!nullSave.error().message.empty());
    QVERIFY(!nullSave.error().recoverable);
    QVERIFY(nullPort.read(CalendarEventType::Vacation).empty());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
invalidStoredColorPassesThroughUnchanged()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
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
    const std::string stored = port.read(CalendarEventType::Other);
    QCOMPARE(stored, utf8(QStringLiteral("not-a-color")));
    QVERIFY(!QColor(QString::fromUtf8(
        stored.data(),
        static_cast<qsizetype>(stored.size())
        )).isValid());
}

void NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests::
saveFailureReturnsTechnicalFailureAndPreservesStoredValues()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    WorkspaceStore* const session = services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const repository = session->settingsRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        QStringLiteral("preserved")
        ));

    ApplicationServicesCalendarEventTypeColorPreferencesPort port(services);
    QVERIFY(port.write(
        CalendarEventType::Vacation,
        utf8(QStringLiteral("#123456"))
        ));
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

    const auto failed = port.write(
        CalendarEventType::Vacation,
        utf8(QStringLiteral("#abcdef"))
        );
    QVERIFY(!failed);
    QCOMPARE(
        failed.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(!failed.error().recoverable);
    QVERIFY(QString::fromStdString(failed.error().message).contains(
        QStringLiteral("forced calendar event color failure")
        ));

    QCOMPARE(
        port.read(CalendarEventType::Vacation),
        utf8(QStringLiteral("#123456"))
        );
    const auto unrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

QTEST_MAIN(
    NextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests
    )

#include "next_platform_application_services_calendar_event_type_color_preferences_port_tests.moc"

#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/calendar_first_day_of_week_preferences.h"
#include "next/platform/application_services_calendar_first_day_of_week_preferences_port.h"

#include <QSqlQuery>
#include <QLocale>
#include <QRegularExpression>
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

constexpr auto FirstDayOfWeekKey = "calendar/firstDayOfWeek";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("calendar-first-day-of-week-%1.tps").arg(
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

CalendarFirstDayOfWeek localeFirstDayOfWeek()
{
    switch (QLocale().firstDayOfWeek())
    {
    case Qt::Sunday:
        return CalendarFirstDayOfWeek::Sunday;
    case Qt::Monday:
        return CalendarFirstDayOfWeek::Monday;
    case Qt::Tuesday:
        return CalendarFirstDayOfWeek::Tuesday;
    case Qt::Wednesday:
        return CalendarFirstDayOfWeek::Wednesday;
    case Qt::Thursday:
        return CalendarFirstDayOfWeek::Thursday;
    case Qt::Friday:
        return CalendarFirstDayOfWeek::Friday;
    case Qt::Saturday:
        return CalendarFirstDayOfWeek::Saturday;
    }

    return CalendarFirstDayOfWeek::Sunday;
}

} // namespace

class NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void missingSettingUsesLocaleFallback();
    void invalidSettingUsesLocaleFallback();
    void roundTripsAllDaysUsingExactKey();
    void unavailableSettingsUseLocaleFallbackAndIgnoreSave();
    void saveFailurePreservesStoredValueAndWarning();

private:
    QLocale m_originalLocale;
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests::
initTestCase()
{
    m_originalLocale = QLocale();
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests::
cleanupTestCase()
{
    QLocale::setDefault(m_originalLocale);
}

void NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests::
missingSettingUsesLocaleFallback()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    ApplicationServices* servicesPointer = &services;
    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort port(
        servicesPointer
        );
    QCOMPARE(port.load(), CalendarFirstDayOfWeek::Sunday);

    QLocale::setDefault(QLocale(QStringLiteral("en_GB")));
    QCOMPARE(port.load(), CalendarFirstDayOfWeek::Monday);
    QCOMPARE(port.load(), localeFirstDayOfWeek());
}

void NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests::
invalidSettingUsesLocaleFallback()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    QLocale::setDefault(QLocale(QStringLiteral("en_GB")));
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(FirstDayOfWeekKey),
            QStringLiteral("not-a-day")
            )
        );

    ApplicationServices* servicesPointer = &services;
    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort port(
        servicesPointer
        );
    QCOMPARE(port.load(), CalendarFirstDayOfWeek::Monday);

    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(FirstDayOfWeekKey),
            7
            )
        );
    QCOMPARE(port.load(), CalendarFirstDayOfWeek::Monday);

    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(FirstDayOfWeekKey),
            -1
            )
        );
    QCOMPARE(port.load(), CalendarFirstDayOfWeek::Monday);
}

void NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests::
roundTripsAllDaysUsingExactKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QStringLiteral("calendar/unrelatedPreference"),
            QStringLiteral("preserved")
            )
        );

    const std::array<CalendarFirstDayOfWeek, 7> values = {{
        CalendarFirstDayOfWeek::Sunday,
        CalendarFirstDayOfWeek::Monday,
        CalendarFirstDayOfWeek::Tuesday,
        CalendarFirstDayOfWeek::Wednesday,
        CalendarFirstDayOfWeek::Thursday,
        CalendarFirstDayOfWeek::Friday,
        CalendarFirstDayOfWeek::Saturday
    }};

    ApplicationServices* servicesPointer = &services;
    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort port(
        servicesPointer
        );
    for (const CalendarFirstDayOfWeek expected : values)
    {
        port.save(expected);
        QCOMPARE(port.load(), expected);

        const auto stored = services.dataService()->loadSetting(
            QString::fromUtf8(FirstDayOfWeekKey)
            );
        QVERIFY(stored);
        QCOMPARE(stored->toInt(), static_cast<int>(expected));
    }

    const auto unrelated = services.dataService()->loadSetting(
        QStringLiteral("calendar/unrelatedPreference")
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toString(), QStringLiteral("preserved"));
}

void NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests::
unavailableSettingsUseLocaleFallbackAndIgnoreSave()
{
    QLocale::setDefault(QLocale(QStringLiteral("en_GB")));
    ApplicationServices services;
    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort port(services);

    QCOMPARE(port.load(), CalendarFirstDayOfWeek::Monday);
    port.save(CalendarFirstDayOfWeek::Sunday);
    QCOMPARE(port.load(), CalendarFirstDayOfWeek::Monday);

    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QCOMPARE(nullPort.load(), CalendarFirstDayOfWeek::Monday);
    nullPort.save(CalendarFirstDayOfWeek::Sunday);
    QCOMPARE(nullPort.load(), CalendarFirstDayOfWeek::Monday);
}

void NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests::
saveFailurePreservesStoredValueAndWarning()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    ApplicationServicesCalendarFirstDayOfWeekPreferencesPort port(services);
    port.save(CalendarFirstDayOfWeek::Sunday);
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_calendar_first_day
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'calendar/firstDayOfWeek'
            BEGIN
                SELECT RAISE(ABORT, 'forced calendar first-day failure');
            END
        )")
        ));

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(
            QStringLiteral(
                "Failed to save calendar first-day preference:.*"
                )
            )
        );
    port.save(CalendarFirstDayOfWeek::Monday);

    QCOMPARE(port.load(), CalendarFirstDayOfWeek::Sunday);
}

QTEST_MAIN(
    NextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests
    )

#include "next_platform_application_services_calendar_first_day_of_week_preferences_port_tests.moc"

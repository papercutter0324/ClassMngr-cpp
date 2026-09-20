#include "core/settingsmanager.h"
#include "next/application/upcoming_birthday_dismissal_port.h"
#include "next/platform/settings_manager_upcoming_birthday_dismissal_port.h"

#include <QDate>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString dismissalKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE
        );
}

} // namespace

class NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactCanonicalKey();
    void convertsTypedDateToLegacyQDate();
    void writesOnlyValidTypedDates();
    void remainsSafeWithUnavailableSettings();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsRoot.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
usesTheExactCanonicalKey()
{
    QCOMPARE(
        dismissalKey(),
        QStringLiteral("notifications/upcomingBirthdaysDismissedDate")
        );
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
convertsTypedDateToLegacyQDate()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerUpcomingBirthdayDismissalPort port;

    port.write(CalendarEventDate("2026-08-21"));

    const QVariant stored = settings.get(dismissalKey());
    QVERIFY(stored.isValid());
    QCOMPARE(stored.toDate(), QDate(2026, 8, 21));
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
writesOnlyValidTypedDates()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerUpcomingBirthdayDismissalPort port;

    settings.set(
        dismissalKey(),
        QDate(2025, 1, 2)
        );

    port.write(CalendarEventDate());
    QCOMPARE(settings.get(dismissalKey()).toDate(), QDate(2025, 1, 2));

    port.write(CalendarEventDate("not-a-date"));
    QCOMPARE(settings.get(dismissalKey()).toDate(), QDate(2025, 1, 2));

    port.write(CalendarEventDate("2026-12-31"));
    QCOMPARE(settings.get(dismissalKey()).toDate(), QDate(2026, 12, 31));
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
remainsSafeWithUnavailableSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.set(dismissalKey(), QVariant());

    const SettingsManagerUpcomingBirthdayDismissalPort port;
    port.write(CalendarEventDate("2027-03-14"));

    QCOMPARE(settings.get(dismissalKey()).toDate(), QDate(2027, 3, 14));
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests
    )

#include "next_platform_settings_manager_upcoming_birthday_dismissal_port_tests.moc"

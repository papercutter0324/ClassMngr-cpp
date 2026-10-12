#include "core/settingsmanager.h"
#include "next/application/upcoming_birthday_dismissal_port.h"
#include "next/platform/settings_manager_upcoming_birthday_dismissal_port.h"

#include <QDate>
#include <QSettings>
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

QSettings::Status syncAndReturnAccessError(QSettings& settings)
{
    settings.sync();
    return QSettings::AccessError;
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
    void failedWriteRestoresExactTypedValueAndPreservesOtherKeys();
    void failedWriteRestoresAnAbsentKey();
    void aLaterWriteRetriesThroughRecreatedSettings();

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

    QVERIFY(port.write(CalendarEventDate("2026-08-21")));

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

    QVERIFY(port.write(CalendarEventDate()));
    QCOMPARE(settings.get(dismissalKey()).toDate(), QDate(2025, 1, 2));

    QVERIFY(port.write(CalendarEventDate("not-a-date")));
    QCOMPARE(settings.get(dismissalKey()).toDate(), QDate(2025, 1, 2));

    QVERIFY(port.write(CalendarEventDate("2026-12-31")));
    QCOMPARE(settings.get(dismissalKey()).toDate(), QDate(2026, 12, 31));
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
remainsSafeWithUnavailableSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.set(dismissalKey(), QVariant());

    const SettingsManagerUpcomingBirthdayDismissalPort port;
    QVERIFY(port.write(CalendarEventDate("2027-03-14")));

    QCOMPARE(settings.get(dismissalKey()).toDate(), QDate(2027, 3, 14));
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
failedWriteRestoresExactTypedValueAndPreservesOtherKeys()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString unrelatedKey = QStringLiteral("files/recent");
    const QVariant previousValue = QByteArray("legacy typed value");
    const QVariant unrelatedValue = QStringList{
        QStringLiteral("D:/one.tps"),
        QStringLiteral("D:/two.tps")
    };
    settings.set(dismissalKey(), previousValue);
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();

    const SettingsManagerUpcomingBirthdayDismissalPort port(
        syncAndReturnAccessError
        );
    const auto result = port.write(CalendarEventDate("2026-10-12"));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().recoverable);
    QVERIFY(!result.error().message.empty());
    QCOMPARE(settings.get(dismissalKey()), previousValue);
    QCOMPARE(settings.get(dismissalKey()).metaType(), previousValue.metaType());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
failedWriteRestoresAnAbsentKey()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString unrelatedKey = QStringLiteral("files/last");
    const QVariant unrelatedValue = QByteArray("keep unrelated value");
    settings.remove(dismissalKey());
    settings.set(unrelatedKey, unrelatedValue);
    settings.sync();
    QVERIFY(!settings.get(dismissalKey()).isValid());

    const SettingsManagerUpcomingBirthdayDismissalPort port(
        syncAndReturnAccessError
        );
    const auto result = port.write(CalendarEventDate("2026-10-12"));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().recoverable);
    QVERIFY(!settings.get(dismissalKey()).isValid());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
}

void NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests::
aLaterWriteRetriesThroughRecreatedSettings()
{
    bool failFirstWrite = true;
    int syncAttempts = 0;
    const SettingsManagerUpcomingBirthdayDismissalPort port(
        [&failFirstWrite, &syncAttempts](QSettings& settings)
        {
            ++syncAttempts;
            settings.sync();
            if (failFirstWrite)
            {
                failFirstWrite = false;
                return QSettings::AccessError;
            }
            return settings.status();
        }
        );

    const auto firstResult = port.write(CalendarEventDate("2026-10-11"));
    QVERIFY(!firstResult);
    QCOMPARE(firstResult.error().code, Domain::ErrorCode::Technical);
    QVERIFY(firstResult.error().recoverable);

    const auto retryResult = port.write(CalendarEventDate("2026-10-12"));
    QVERIFY(retryResult);
    QCOMPARE(syncAttempts, 2);
    QCOMPARE(
        SettingsManager::instance().get(dismissalKey()).toDate(),
        QDate(2026, 10, 12)
        );
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerUpcomingBirthdayDismissalPortTests
    )

#include "next_platform_settings_manager_upcoming_birthday_dismissal_port_tests.moc"

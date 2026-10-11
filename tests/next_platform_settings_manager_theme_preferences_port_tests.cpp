#include "core/settingsmanager.h"
#include "next/application/theme_preferences_port.h"
#include "next/platform/settings_manager_theme_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString themeKey()
{
    return QString::fromUtf8(
        OptionKeys::Theme
        );
}

} // namespace

class NextPlatformSettingsManagerThemePreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactCanonicalKey();
    void mapsAllStoredThemeValues();
    void writesCanonicalThemeValues();
    void themeSyncFailurePreservesPreviousValueAndAllowsRetry();
    void missingAndUnknownValuesDefaultToSystemDefault();
    void unavailableSettingsDefaultToSystemDefault();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerThemePreferencesPortTests::
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

void NextPlatformSettingsManagerThemePreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerThemePreferencesPortTests::
usesTheExactCanonicalKey()
{
    QCOMPARE(
        themeKey(),
        QStringLiteral("options/theme")
        );

    SettingsManager::instance().set(
        themeKey(),
        0
        );

    const SettingsManagerThemePreferencesPort port;
    QCOMPARE(
        port.read(),
        Theme::Dark
        );
}

void NextPlatformSettingsManagerThemePreferencesPortTests::
mapsAllStoredThemeValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerThemePreferencesPort port;

    const Theme expectedThemes[] = {
        Theme::Dark,
        Theme::Light,
        Theme::SystemDefault
    };

    for (int storedValue = 0; storedValue < 3; ++storedValue)
    {
        settings.set(themeKey(), storedValue);
        QCOMPARE(
            port.read(),
            expectedThemes[storedValue]
            );
    }
}

void NextPlatformSettingsManagerThemePreferencesPortTests::
writesCanonicalThemeValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerThemePreferencesPort port;

    const Theme themes[] = {
        Theme::Dark,
        Theme::Light,
        Theme::SystemDefault
    };

    for (int storedValue = 0; storedValue < 3; ++storedValue)
    {
        const auto result = port.write(themes[storedValue]);
        QVERIFY(result);
        QCOMPARE(
            settings.get(themeKey()).toInt(),
            storedValue
            );
    }

    const auto invalidResult = port.write(static_cast<Theme>(17));
    QVERIFY(!invalidResult);
    QCOMPARE(
        invalidResult.error().code,
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );
    QCOMPARE(
        settings.get(themeKey()).toInt(),
        2
        );
}

void NextPlatformSettingsManagerThemePreferencesPortTests::
themeSyncFailurePreservesPreviousValueAndAllowsRetry()
{
    const SettingsManagerThemePreferencesPort normalPort;
    const auto seedResult = normalPort.write(Theme::Dark);
    QVERIFY(seedResult);
    QCOMPARE(normalPort.read(), Theme::Dark);

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerThemePreferencesPort faultablePort(
        [&injectAccessError, &synchronizationAttempts](QSettings& settings)
        {
            ++synchronizationAttempts;
            if (injectAccessError)
            {
                return QSettings::AccessError;
            }

            settings.sync();
            return settings.status();
        }
        );

    const auto failedWrite = faultablePort.write(Theme::Light);
    QVERIFY(!failedWrite);
    QCOMPARE(
        failedWrite.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(faultablePort.read(), Theme::Dark);

    injectAccessError = false;
    const auto retryResult = faultablePort.write(Theme::Light);
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(faultablePort.read(), Theme::Light);
}

void NextPlatformSettingsManagerThemePreferencesPortTests::
missingAndUnknownValuesDefaultToSystemDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerThemePreferencesPort port;

    settings.remove(themeKey());
    QCOMPARE(
        port.read(),
        Theme::SystemDefault
        );

    settings.set(themeKey(), -1);
    QCOMPARE(
        port.read(),
        Theme::SystemDefault
        );

    settings.set(themeKey(), 3);
    QCOMPARE(
        port.read(),
        Theme::SystemDefault
        );

    settings.set(themeKey(), QStringLiteral("unknown"));
    QCOMPARE(
        port.read(),
        Theme::SystemDefault
        );
}

void NextPlatformSettingsManagerThemePreferencesPortTests::
unavailableSettingsDefaultToSystemDefault()
{
    SettingsManager::instance().set(
        themeKey(),
        QVariant()
        );

    const SettingsManagerThemePreferencesPort port;
    QCOMPARE(
        port.read(),
        Theme::SystemDefault
        );
    QVERIFY(!SettingsManager::instance().get(themeKey()).isValid());
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerThemePreferencesPortTests
    )

#include "next_platform_settings_manager_theme_preferences_port_tests.moc"

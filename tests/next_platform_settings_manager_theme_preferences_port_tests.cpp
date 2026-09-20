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

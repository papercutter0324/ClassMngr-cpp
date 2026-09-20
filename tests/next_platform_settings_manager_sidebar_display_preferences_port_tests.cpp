#include "core/settingsmanager.h"
#include "next/application/sidebar_display_preferences.h"
#include "next/platform/settings_manager_sidebar_display_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString settingsKey(const char* key)
{
    return QString::fromUtf8(key);
}

QString sidebarTooltipsKey()
{
    return settingsKey(SettingsManager::Keys::SIDEBAR_TOOLTIPS_ENABLED);
}

QString sidebarMarqueeKey()
{
    return settingsKey(SettingsManager::Keys::SIDEBAR_MARQUEE_ENABLED);
}

} // namespace

class NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void missingSettingsUseEnabledDefaults();
    void readsBothLegacyKeys();
    void preservesLegacyQVariantCoercion();
    void invalidSettingsUseEnabledDefaults();
    void roundTripsBothValuesAndKeys();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::
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

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::
missingSettingsUseEnabledDefaults()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(sidebarTooltipsKey());
    settings.remove(sidebarMarqueeKey());

    const SettingsManagerSidebarDisplayPreferencesPort port;
    const auto result = port.load();

    QVERIFY(result);
    QVERIFY(result.value().sidebarTooltipsEnabled);
    QVERIFY(result.value().sidebarMarqueeEnabled);
}

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::
readsBothLegacyKeys()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.set(sidebarTooltipsKey(), false);
    settings.set(sidebarMarqueeKey(), true);

    const SettingsManagerSidebarDisplayPreferencesPort port;
    const auto result = port.load();

    QVERIFY(result);
    QVERIFY(!result.value().sidebarTooltipsEnabled);
    QVERIFY(result.value().sidebarMarqueeEnabled);
    QCOMPARE(
        sidebarTooltipsKey(),
        QStringLiteral("options/sidebarTooltipsEnabled")
        );
    QCOMPARE(
        sidebarMarqueeKey(),
        QStringLiteral("options/sidebarMarqueeEnabled")
        );
}

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::
preservesLegacyQVariantCoercion()
{
    SettingsManager& settings = SettingsManager::instance();
    const std::array<std::pair<QVariant, bool>, 6> values = {{
        {QVariant(QStringLiteral("true")), true},
        {QVariant(QStringLiteral("false")), false},
        {QVariant(1), true},
        {QVariant(0), false},
        {QVariant(true), true},
        {QVariant(false), false}
    }};

    const SettingsManagerSidebarDisplayPreferencesPort port;
    for (const auto& [value, expected] : values)
    {
        settings.set(sidebarTooltipsKey(), value);
        settings.set(sidebarMarqueeKey(), true);
        auto result = port.load();
        QVERIFY(result);
        QCOMPARE(result.value().sidebarTooltipsEnabled, expected);

        settings.set(sidebarTooltipsKey(), true);
        settings.set(sidebarMarqueeKey(), value);
        result = port.load();
        QVERIFY(result);
        QCOMPARE(result.value().sidebarMarqueeEnabled, expected);
    }
}

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::
invalidSettingsUseEnabledDefaults()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.set(sidebarTooltipsKey(), QVariant());
    settings.set(sidebarMarqueeKey(), QVariant());

    const SettingsManagerSidebarDisplayPreferencesPort port;
    const auto result = port.load();

    // SettingsManager has no availability/status surface; invalid reads model
    // the unavailable legacy value at this boundary.
    QVERIFY(result);
    QVERIFY(result.value().sidebarTooltipsEnabled);
    QVERIFY(result.value().sidebarMarqueeEnabled);
}

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::
roundTripsBothValuesAndKeys()
{
    SettingsManager& settings = SettingsManager::instance();
    SettingsManagerSidebarDisplayPreferencesPort port;
    const std::array<SidebarDisplayPreferences, 4> values = {{
        {.sidebarTooltipsEnabled = true, .sidebarMarqueeEnabled = true},
        {.sidebarTooltipsEnabled = true, .sidebarMarqueeEnabled = false},
        {.sidebarTooltipsEnabled = false, .sidebarMarqueeEnabled = true},
        {.sidebarTooltipsEnabled = false, .sidebarMarqueeEnabled = false}
    }};

    for (const SidebarDisplayPreferences& expected : values)
    {
        QVERIFY(port.save(expected));
        settings.sync();

        const auto loaded = port.load();
        QVERIFY(loaded);
        QVERIFY(loaded.value() == expected);
        QCOMPARE(
            settings.get(sidebarTooltipsKey()).toBool(),
            expected.sidebarTooltipsEnabled
            );
        QCOMPARE(
            settings.get(sidebarMarqueeKey()).toBool(),
            expected.sidebarMarqueeEnabled
            );
    }
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests)

#include "next_platform_settings_manager_sidebar_display_preferences_port_tests.moc"

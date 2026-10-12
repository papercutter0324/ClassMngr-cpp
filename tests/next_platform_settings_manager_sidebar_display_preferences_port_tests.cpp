#include "core/settingsmanager.h"
#include "next/application/sidebar_display_preferences.h"
#include "next/platform/settings_manager_sidebar_display_preferences_port.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <string>
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
    void syncFailureRestoresPreviousValuesAndAllowsRetry();
    void syncFailureRestoresBothAbsentKeysAndPreservesUnrelatedSetting();

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

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::
syncFailureRestoresPreviousValuesAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString tooltipsKey = sidebarTooltipsKey();
    const QString marqueeKey = sidebarMarqueeKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    const QVariant previousTooltips = QStringLiteral("false");
    const QVariant previousMarquee = 0;
    settings.set(tooltipsKey, previousTooltips);
    settings.set(marqueeKey, previousMarquee);
    settings.set(unrelatedKey, QStringLiteral("keep-me"));
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    SettingsManagerSidebarDisplayPreferencesPort faultablePort(
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

    const SidebarDisplayPreferencesSaveRequest rejectedRequest{
        .sidebarTooltipsEnabled = true,
        .sidebarMarqueeEnabled = true
    };
    const auto failedSave = faultablePort.save(rejectedRequest);
    QVERIFY(!failedSave);
    QCOMPARE(failedSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedSave.error().recoverable);
    QVERIFY(
        failedSave.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(tooltipsKey), previousTooltips);
    QCOMPARE(settings.get(marqueeKey), previousMarquee);
    QCOMPARE(settings.get(unrelatedKey), QVariant(QStringLiteral("keep-me")));
    const auto restored = faultablePort.load();
    QVERIFY(restored);
    QVERIFY(!restored.value().sidebarTooltipsEnabled);
    QVERIFY(!restored.value().sidebarMarqueeEnabled);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(tooltipsKey), previousTooltips);
    QCOMPARE(persistedSettings.value(marqueeKey), previousMarquee);
    QCOMPARE(
        persistedSettings.value(unrelatedKey),
        QVariant(QStringLiteral("keep-me"))
        );

    injectAccessError = false;
    const SidebarDisplayPreferencesSaveRequest retryRequest{
        .sidebarTooltipsEnabled = false,
        .sidebarMarqueeEnabled = true
    };
    const auto retryResult = faultablePort.save(retryRequest);
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    const auto retried = faultablePort.load();
    QVERIFY(retried);
    QVERIFY(retried.value() == retryRequest);
    QCOMPARE(settings.get(tooltipsKey).toBool(), false);
    QCOMPARE(settings.get(marqueeKey).toBool(), true);
    QCOMPARE(settings.get(unrelatedKey), QVariant(QStringLiteral("keep-me")));
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(tooltipsKey).toBool(), false);
    QCOMPARE(persistedSettings.value(marqueeKey).toBool(), true);
    QCOMPARE(
        persistedSettings.value(unrelatedKey),
        QVariant(QStringLiteral("keep-me"))
        );
}

void NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests::
syncFailureRestoresBothAbsentKeysAndPreservesUnrelatedSetting()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString tooltipsKey = sidebarTooltipsKey();
    const QString marqueeKey = sidebarMarqueeKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.remove(tooltipsKey);
    settings.remove(marqueeKey);
    settings.set(unrelatedKey, 73);
    settings.sync();

    SettingsManagerSidebarDisplayPreferencesPort faultablePort(
        [](QSettings&)
        {
            return QSettings::AccessError;
        }
        );
    const auto failedSave = faultablePort.save({
        .sidebarTooltipsEnabled = false,
        .sidebarMarqueeEnabled = false
    });
    QVERIFY(!failedSave);
    QCOMPARE(failedSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedSave.error().recoverable);
    QVERIFY(!settings.get(tooltipsKey).isValid());
    QVERIFY(!settings.get(marqueeKey).isValid());
    QCOMPARE(settings.get(unrelatedKey).toInt(), 73);
    const auto restored = faultablePort.load();
    QVERIFY(restored);
    QVERIFY(restored.value().sidebarTooltipsEnabled);
    QVERIFY(restored.value().sidebarMarqueeEnabled);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(tooltipsKey));
    QVERIFY(!persistedSettings.contains(marqueeKey));
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 73);
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerSidebarDisplayPreferencesPortTests)

#include "next_platform_settings_manager_sidebar_display_preferences_port_tests.moc"

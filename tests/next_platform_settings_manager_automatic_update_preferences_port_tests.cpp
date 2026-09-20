#include "core/settingsmanager.h"
#include "next/application/automatic_update_preferences.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString automaticChecksEnabledKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED
        );
}

} // namespace

class NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void missingSettingUsesEnabledDefault();
    void preservesLegacySettingsKey();
    void preservesLegacyQVariantCoercion();
    void unavailableSettingUsesEnabledDefault();
    void roundTripsThroughSettingsManager();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::
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

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::
missingSettingUsesEnabledDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(automaticChecksEnabledKey());

    const SettingsManagerAutomaticUpdatePreferencesPort port;
    const AutomaticUpdatePreferences preferences = port.read();

    QVERIFY(preferences.automaticChecksEnabled);
    QVERIFY(!settings.get(automaticChecksEnabledKey()).isValid());
}

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::
preservesLegacySettingsKey()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = automaticChecksEnabledKey();
    const SettingsManagerAutomaticUpdatePreferencesPort port;

    QCOMPARE(key, QStringLiteral("updates/automaticChecksEnabled"));
    settings.set(QStringLiteral("updates/unrelatedValue"), 17);

    port.write({.automaticChecksEnabled = false});

    QCOMPARE(settings.get(key).toBool(), false);
    QCOMPARE(
        settings.get(QStringLiteral("updates/unrelatedValue")).toInt(),
        17
        );
}

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::
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

    const SettingsManagerAutomaticUpdatePreferencesPort port;
    for (const auto& [value, expected] : values)
    {
        settings.set(automaticChecksEnabledKey(), value);
        QCOMPARE(port.read().automaticChecksEnabled, expected);
    }
}

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::
unavailableSettingUsesEnabledDefault()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.set(automaticChecksEnabledKey(), QVariant());

    const SettingsManagerAutomaticUpdatePreferencesPort port;
    QVERIFY(port.read().automaticChecksEnabled);
}

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::
roundTripsThroughSettingsManager()
{
    const SettingsManagerAutomaticUpdatePreferencesPort port;

    for (const bool expected : {true, false})
    {
        port.write({.automaticChecksEnabled = expected});
        const AutomaticUpdatePreferences loaded = port.read();

        QCOMPARE(loaded.automaticChecksEnabled, expected);
        QCOMPARE(
            SettingsManager::instance()
                .get(automaticChecksEnabledKey())
                .toBool(),
            expected
            );
    }
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests
    )

#include "next_platform_settings_manager_automatic_update_preferences_port_tests.moc"

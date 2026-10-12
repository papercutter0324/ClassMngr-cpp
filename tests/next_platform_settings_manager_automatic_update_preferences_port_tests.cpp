#include "core/settingsmanager.h"
#include "next/application/automatic_update_preferences.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"

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
    void syncFailurePreservesPreviousValueAndAllowsRetry();
    void syncFailurePreservesMissingValueAndUnrelatedSettings();

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

    QVERIFY(port.write({.automaticChecksEnabled = false}));

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
        QVERIFY(port.write({.automaticChecksEnabled = expected}));
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

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::
syncFailurePreservesPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = automaticChecksEnabledKey();
    const QString unrelatedKey = QStringLiteral("updates/unrelatedValue");
    settings.set(key, true);
    settings.set(unrelatedKey, 41);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerAutomaticUpdatePreferencesPort faultablePort(
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

    const auto failedWrite = faultablePort.write({
        .automaticChecksEnabled = false
    });
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
    QVERIFY(settings.get(key).toBool());
    QVERIFY(faultablePort.read().automaticChecksEnabled);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(persistedSettings.value(key).toBool());
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);

    injectAccessError = false;
    const auto retryResult = faultablePort.write({
        .automaticChecksEnabled = false
    });
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    QVERIFY(!settings.get(key).toBool());
    QVERIFY(!faultablePort.read().automaticChecksEnabled);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.value(key).toBool());
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);
}

void NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests::
syncFailurePreservesMissingValueAndUnrelatedSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = automaticChecksEnabledKey();
    const QString unrelatedKey = QStringLiteral("updates/unrelatedValue");
    settings.remove(key);
    settings.set(unrelatedKey, 29);
    settings.sync();

    const SettingsManagerAutomaticUpdatePreferencesPort faultablePort(
        [](QSettings&)
        {
            return QSettings::AccessError;
        }
        );

    const auto failedWrite = faultablePort.write({
        .automaticChecksEnabled = false
    });
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
    QVERIFY(!settings.get(key).isValid());
    QVERIFY(faultablePort.read().automaticChecksEnabled);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 29);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 29);
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerAutomaticUpdatePreferencesPortTests
    )

#include "next_platform_settings_manager_automatic_update_preferences_port_tests.moc"

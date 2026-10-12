#include "core/settingsmanager.h"
#include "next/application/ai_comment_provider_preferences.h"
#include "next/platform/settings_manager_ai_comment_provider_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString providerKey()
{
    return QString::fromUtf8(
        OptionKeys::AiCommentProvider
        );
}

} // namespace

class NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactLegacyKey();
    void mapsAllStoredProviderValues();
    void writesAllProviderValuesForRoundTrip();
    void missingAndUnknownValuesDefaultToChatGPT();
    void unavailableSettingsDefaultToChatGPT();
    void syncFailurePreservesPreviousValueAndAllowsRetry();
    void syncFailurePreservesMissingValueAndUnrelatedSettings();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::
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

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::
usesTheExactLegacyKey()
{
    QCOMPARE(
        providerKey(),
        QStringLiteral("options/aiCommentProvider")
        );

    SettingsManager::instance().set(
        providerKey(),
        4
        );

    const SettingsManagerAiCommentProviderPreferencesPort port;
    QCOMPARE(
        port.read(),
        AiCommentProvider::CustomWebsite
        );
}

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::
mapsAllStoredProviderValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerAiCommentProviderPreferencesPort port;

    const AiCommentProvider expectedProviders[] = {
        AiCommentProvider::ChatGPT,
        AiCommentProvider::Gemini,
        AiCommentProvider::Claude,
        AiCommentProvider::MicrosoftCopilot,
        AiCommentProvider::CustomWebsite
    };

    for (int storedValue = 0; storedValue < 5; ++storedValue)
    {
        settings.set(providerKey(), storedValue);
        QCOMPARE(
            port.read(),
            expectedProviders[storedValue]
            );
    }
}

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::
writesAllProviderValuesForRoundTrip()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerAiCommentProviderPreferencesPort port;

    const AiCommentProvider expectedProviders[] = {
        AiCommentProvider::ChatGPT,
        AiCommentProvider::Gemini,
        AiCommentProvider::Claude,
        AiCommentProvider::MicrosoftCopilot,
        AiCommentProvider::CustomWebsite
    };

    for (int providerIndex = 0; providerIndex < 5; ++providerIndex)
    {
        QVERIFY(port.write(expectedProviders[providerIndex]));
        QCOMPARE(
            settings.get(providerKey()).toInt(),
            providerIndex
            );
        QCOMPARE(
            port.read(),
            expectedProviders[providerIndex]
            );
    }

    const auto invalidWrite = port.write(
        static_cast<AiCommentProvider>(99)
        );
    QVERIFY(!invalidWrite);
    QCOMPARE(
        invalidWrite.error().code,
        Domain::ErrorCode::InvalidInput
        );
    QCOMPARE(
        settings.get(providerKey()).toInt(),
        4
        );
}

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::
missingAndUnknownValuesDefaultToChatGPT()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerAiCommentProviderPreferencesPort port;

    settings.remove(providerKey());
    QCOMPARE(
        port.read(),
        AiCommentProvider::ChatGPT
        );

    settings.set(providerKey(), -1);
    QCOMPARE(
        port.read(),
        AiCommentProvider::ChatGPT
        );

    settings.set(providerKey(), 5);
    QCOMPARE(
        port.read(),
        AiCommentProvider::ChatGPT
        );

    settings.set(providerKey(), QStringLiteral("unknown"));
    QCOMPARE(
        port.read(),
        AiCommentProvider::ChatGPT
        );
}

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::
unavailableSettingsDefaultToChatGPT()
{
    SettingsManager::instance().set(
        providerKey(),
        QVariant()
        );

    const SettingsManagerAiCommentProviderPreferencesPort port;
    QCOMPARE(
        port.read(),
        AiCommentProvider::ChatGPT
        );
    QVERIFY(!SettingsManager::instance().get(providerKey()).isValid());
}

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::
syncFailurePreservesPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = providerKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.set(key, 1);
    settings.set(unrelatedKey, 41);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerAiCommentProviderPreferencesPort faultablePort(
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

    const auto failedWrite = faultablePort.write(AiCommentProvider::Claude);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(key).toInt(), 1);
    QCOMPARE(faultablePort.read(), AiCommentProvider::Gemini);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key).toInt(), 1);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);

    injectAccessError = false;
    const auto retryResult = faultablePort.write(AiCommentProvider::Claude);
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key).toInt(), 2);
    QCOMPARE(faultablePort.read(), AiCommentProvider::Claude);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key).toInt(), 2);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);
}

void NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests::
syncFailurePreservesMissingValueAndUnrelatedSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = providerKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.remove(key);
    settings.set(unrelatedKey, 29);
    settings.sync();

    const SettingsManagerAiCommentProviderPreferencesPort faultablePort(
        [](QSettings&)
        {
            return QSettings::AccessError;
        }
        );

    const auto failedWrite =
        faultablePort.write(AiCommentProvider::MicrosoftCopilot);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QVERIFY(!settings.get(key).isValid());
    QCOMPARE(faultablePort.read(), AiCommentProvider::ChatGPT);
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
    NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests
    )

#include "next_platform_settings_manager_ai_comment_provider_preferences_port_tests.moc"

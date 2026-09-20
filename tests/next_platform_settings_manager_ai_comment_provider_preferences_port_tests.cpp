#include "core/settingsmanager.h"
#include "next/application/ai_comment_provider_preferences.h"
#include "next/platform/settings_manager_ai_comment_provider_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

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
    void missingAndUnknownValuesDefaultToChatGPT();
    void unavailableSettingsDefaultToChatGPT();

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

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerAiCommentProviderPreferencesPortTests
    )

#include "next_platform_settings_manager_ai_comment_provider_preferences_port_tests.moc"

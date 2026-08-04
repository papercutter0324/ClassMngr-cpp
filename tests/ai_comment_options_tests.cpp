#include "core/settingsmanager.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/state/ai_comment_options.h"
#include "ui/shared/state/option_state.h"
#include "ui/shared/state/option_state_keys.h"

#include <QtTest>

#include <QTemporaryDir>

class AiCommentOptionsTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void providerAndVoiceDefaultsPersist();
    void providerUrlsAndCustomValidation();

private:
    QTemporaryDir m_settingsRoot;
};

void AiCommentOptionsTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
    SettingsManager::instance().clear();
}

void AiCommentOptionsTests::
    providerAndVoiceDefaultsPersist()
{
    SettingsManager& settings =
        SettingsManager::instance();
    settings.remove(
        QString::fromUtf8(
            OptionKeys::AiCommentProvider
            )
        );
    settings.remove(
        QString::fromUtf8(
            OptionKeys::AiCommentVoice
            )
        );

    ActionRegistry defaults;
    defaults.createActions();
    QVERIFY(defaults.aiCommentProviderState);
    QVERIFY(defaults.aiCommentVoiceState);
    QCOMPARE(
        defaults.aiCommentProviderState->current(),
        AiCommentProvider::ChatGPT
        );
    QCOMPARE(
        defaults.aiCommentVoiceState->current(),
        AiCommentVoice::DirectToStudent
        );

    defaults.aiCommentProviderState->set(
        AiCommentProvider::Claude
        );
    defaults.aiCommentVoiceState->set(
        AiCommentVoice::ThirdPerson
        );
    settings.sync();

    ActionRegistry reloaded;
    reloaded.createActions();
    QCOMPARE(
        reloaded.aiCommentProviderState->current(),
        AiCommentProvider::Claude
        );
    QCOMPARE(
        reloaded.aiCommentVoiceState->current(),
        AiCommentVoice::ThirdPerson
        );
}

void AiCommentOptionsTests::
    providerUrlsAndCustomValidation()
{
    QCOMPARE(
        aiCommentProviderUrl(
            AiCommentProvider::ChatGPT
            ).toString(),
        QStringLiteral("https://chatgpt.com/")
        );
    QCOMPARE(
        aiCommentProviderUrl(
            AiCommentProvider::Gemini
            ).host(),
        QStringLiteral("gemini.google.com")
        );
    QCOMPARE(
        aiCommentProviderUrl(
            AiCommentProvider::Claude
            ).host(),
        QStringLiteral("claude.ai")
        );
    QCOMPARE(
        aiCommentProviderUrl(
            AiCommentProvider::MicrosoftCopilot
            ).host(),
        QStringLiteral("copilot.microsoft.com")
        );

    QVERIFY(
        isValidCustomAiWebsiteUrl(
            QStringLiteral("https://example.ai/chat")
            )
        );
    QVERIFY(
        !isValidCustomAiWebsiteUrl(
            QStringLiteral("http://example.ai/chat")
            )
        );
    QVERIFY(
        !isValidCustomAiWebsiteUrl(
            QStringLiteral("not a website")
            )
        );
    QCOMPARE(
        aiCommentProviderUrl(
            AiCommentProvider::CustomWebsite,
            QStringLiteral("https://example.ai/chat")
            ).toString(),
        QStringLiteral("https://example.ai/chat")
        );
    QVERIFY(
        aiCommentProviderUrl(
            AiCommentProvider::CustomWebsite,
            QStringLiteral("http://example.ai")
            ).isEmpty()
        );
}

QTEST_MAIN(AiCommentOptionsTests)

#include "ai_comment_options_tests.moc"

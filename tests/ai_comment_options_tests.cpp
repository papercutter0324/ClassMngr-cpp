#include "core/settingsmanager.h"
#include "next/platform/settings_manager_ai_comment_custom_website_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/state/ai_comment_options.h"
#include "ui/shared/state/option_state.h"
#include "ui/shared/state/option_state_keys.h"

#include <QtTest>

#include <QTemporaryDir>

#include <string>

class AiCommentOptionsTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void themeDefaultsToSystemDefaultAndPersists();
    void saveModeStartupAndWritesCanonicalValues();
    void providerAndVoiceDefaultsPersist();
    void documentViewerBackgroundStartupReloadAndCanonicalPersistence();
    void customWebsitePersistenceAndInvalidFallback();
    void providerUrlsAndCustomValidation();
    void updatePreferencesDefaultAndPersist();
    void sidebarDisplayDefaultsAndPersist();
    void excelImportTimeoutDefaultsAndPersists();

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

void AiCommentOptionsTests::themeDefaultsToSystemDefaultAndPersists()
{
    SettingsManager& settings =
        SettingsManager::instance();
    settings.remove(
        QString::fromUtf8(
            OptionKeys::Theme
            )
        );

    ActionRegistry defaults;
    defaults.createActions();
    QVERIFY(defaults.themeState);
    QCOMPARE(
        defaults.themeState->current(),
        Theme::SystemDefault
        );
    QVERIFY(
        defaults.themeState
            ->action(Theme::SystemDefault)
            ->isChecked()
        );

    defaults.themeState->set(Theme::Light);
    settings.sync();

    ActionRegistry reloaded;
    reloaded.createActions();
    QCOMPARE(
        reloaded.themeState->current(),
        Theme::Light
        );
}

void AiCommentOptionsTests::saveModeStartupAndWritesCanonicalValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString saveModeKey =
        QString::fromUtf8(OptionKeys::SaveMode);
    const QString themeKey =
        QString::fromUtf8(OptionKeys::Theme);
    const QVariant savedSaveMode = settings.get(saveModeKey);
    const QVariant savedTheme = settings.get(themeKey);

    settings.set(saveModeKey, 1);
    settings.set(themeKey, 0);

    ActionRegistry manual;
    manual.createActions();
    QCOMPARE(manual.saveModeState->current(), SaveMode::Manual);
    QCOMPARE(settings.get(saveModeKey).toInt(), 1);

    settings.set(saveModeKey, 0);
    ActionRegistry automatic;
    automatic.createActions();
    QCOMPARE(automatic.saveModeState->current(), SaveMode::Automatic);
    QCOMPARE(settings.get(saveModeKey).toInt(), 0);

    automatic.saveModeState->set(SaveMode::Manual);
    QCOMPARE(settings.get(saveModeKey).toInt(), 1);
    automatic.saveModeState->set(SaveMode::Automatic);
    QCOMPARE(settings.get(saveModeKey).toInt(), 0);

    automatic.themeState->set(Theme::Light);
    QCOMPARE(settings.get(themeKey).toInt(), 1);

    if (savedSaveMode.isValid())
    {
        settings.set(saveModeKey, savedSaveMode);
    }
    else
    {
        settings.remove(saveModeKey);
    }

    if (savedTheme.isValid())
    {
        settings.set(themeKey, savedTheme);
    }
    else
    {
        settings.remove(themeKey);
    }
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
    const QString providerKey =
        QString::fromUtf8(OptionKeys::AiCommentProvider);
    const QString voiceKey =
        QString::fromUtf8(OptionKeys::AiCommentVoice);
    QVERIFY(defaults.aiCommentProviderState);
    QVERIFY(defaults.aiCommentVoiceState);
    QCOMPARE(
        defaults.aiCommentProviderState->current(),
        AiCommentProvider::ChatGPT
        );
    QCOMPARE(settings.get(providerKey).toInt(), 0);
    QCOMPARE(
        defaults.aiCommentVoiceState->current(),
        AiCommentVoice::DirectToStudent
        );

    defaults.aiCommentProviderState->set(
        AiCommentProvider::Claude
        );
    QCOMPARE(settings.get(providerKey).toInt(), 2);
    defaults.aiCommentVoiceState->set(
        AiCommentVoice::ThirdPerson
        );
    QCOMPARE(settings.get(voiceKey).toInt(), 1);
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

    reloaded.aiCommentProviderState->set(
        AiCommentProvider::ChatGPT
        );
    QCOMPARE(settings.get(providerKey).toInt(), 0);
    settings.sync();

    ActionRegistry reloadedDefaultProvider;
    reloadedDefaultProvider.createActions();
    QCOMPARE(
        reloadedDefaultProvider.aiCommentProviderState->current(),
        AiCommentProvider::ChatGPT
        );

    reloaded.aiCommentVoiceState->set(
        AiCommentVoice::DirectToStudent
        );
    QCOMPARE(settings.get(voiceKey).toInt(), 0);
    settings.sync();

    ActionRegistry reloadedDirect;
    reloadedDirect.createActions();
    QCOMPARE(
        reloadedDirect.aiCommentVoiceState->current(),
        AiCommentVoice::DirectToStudent
        );
}

void AiCommentOptionsTests::
    documentViewerBackgroundStartupReloadAndCanonicalPersistence()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString backgroundKey =
        QString::fromUtf8(OptionKeys::DocumentViewerBackground);
    settings.remove(backgroundKey);

    ActionRegistry defaults;
    defaults.createActions();
    QVERIFY(defaults.documentViewerBackgroundState);
    QCOMPARE(
        defaults.documentViewerBackgroundState->current(),
        DocumentViewerBackground::Default
        );
    QCOMPARE(settings.get(backgroundKey).toInt(), 0);

    const DocumentViewerBackground expectedValues[] = {
        DocumentViewerBackground::White,
        DocumentViewerBackground::Black,
        DocumentViewerBackground::Default
    };
    const int expectedStoredValues[] = {1, 2, 0};

    for (int index = 0; index < 3; ++index)
    {
        defaults.documentViewerBackgroundState->set(
            expectedValues[index]
            );
        QCOMPARE(
            settings.get(backgroundKey).toInt(),
            expectedStoredValues[index]
            );
        settings.sync();

        ActionRegistry reloaded;
        reloaded.createActions();
        QCOMPARE(
            reloaded.documentViewerBackgroundState->current(),
            expectedValues[index]
            );
    }
}

void AiCommentOptionsTests::
    customWebsitePersistenceAndInvalidFallback()
{
    SettingsManager& settings =
        SettingsManager::instance();
    const QString providerKey =
        QString::fromUtf8(OptionKeys::AiCommentProvider);
    const ClassMngr::Next::Platform::
        SettingsManagerAiCommentCustomWebsitePort websitePort;

    websitePort.write(
        std::string("https://example.ai/chat")
        );
    settings.set(
        providerKey,
        static_cast<int>(AiCommentProvider::CustomWebsite)
        );

    ActionRegistry valid;
    valid.createActions();
    QCOMPARE(
        valid.aiCommentProviderState->current(),
        AiCommentProvider::CustomWebsite
        );

    websitePort.write(std::string("not a website"));
    settings.set(
        providerKey,
        static_cast<int>(AiCommentProvider::CustomWebsite)
        );

    ActionRegistry invalid;
    invalid.createActions();
    QCOMPARE(
        invalid.aiCommentProviderState->current(),
        AiCommentProvider::ChatGPT
        );
    const std::string storedInvalidWebsite =
        websitePort.read();
    QCOMPARE(
        QString::fromUtf8(
            storedInvalidWebsite.data(),
            static_cast<qsizetype>(storedInvalidWebsite.size())
            ),
        QStringLiteral("not a website")
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

void AiCommentOptionsTests::
    updatePreferencesDefaultAndPersist()
{
    SettingsManager& settings =
        SettingsManager::instance();
    settings.remove(
        QString::fromUtf8(
            OptionKeys::AutomaticUpdateChecksEnabled
            )
        );
    settings.clearSkippedUpdateVersion();

    ActionRegistry defaults;
    defaults.createActions();
    QVERIFY(defaults.automaticallyCheckForUpdates);
    QVERIFY(
        defaults.automaticallyCheckForUpdates->isChecked()
        );
    QVERIFY(
        !settings.get(
            QString::fromUtf8(
                SettingsManager::Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED
                )
            ).isValid()
        );

    defaults.automaticallyCheckForUpdates->setChecked(false);
    QVERIFY(
        !settings.get(
            QString::fromUtf8(
                SettingsManager::Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED
                )
            ).toBool()
        );

    ActionRegistry reloaded;
    reloaded.createActions();
    QVERIFY(
        !reloaded.automaticallyCheckForUpdates->isChecked()
        );

    reloaded.automaticallyCheckForUpdates->setChecked(true);
    ActionRegistry reloadedAgain;
    reloadedAgain.createActions();
    QVERIFY(
        reloadedAgain.automaticallyCheckForUpdates->isChecked()
        );

    settings.setSkippedUpdateVersion(
        QStringLiteral(" 2.4.1 ")
        );
    QCOMPARE(
        settings.skippedUpdateVersion(),
        QStringLiteral("2.4.1")
        );
    settings.clearSkippedUpdateVersion();
    QVERIFY(settings.skippedUpdateVersion().isEmpty());
}

void AiCommentOptionsTests::excelImportTimeoutDefaultsAndPersists()
{
    SettingsManager& settings =
        SettingsManager::instance();
    settings.remove(
        QString::fromUtf8(
            SettingsManager::Keys::EXCEL_IMPORT_TIMEOUT_SECONDS
            )
        );
    QCOMPARE(settings.excelImportTimeoutSeconds(), 120);

    settings.setExcelImportTimeoutSeconds(30);
    QCOMPARE(settings.excelImportTimeoutSeconds(), 30);

    settings.setExcelImportTimeoutSeconds(60);
    QCOMPARE(settings.excelImportTimeoutSeconds(), 60);

    settings.setExcelImportTimeoutSeconds(120);
    QCOMPARE(settings.excelImportTimeoutSeconds(), 120);

    settings.setExcelImportTimeoutSeconds(300);
    QCOMPARE(settings.excelImportTimeoutSeconds(), 300);

    settings.setExcelImportTimeoutSeconds(90);
    QCOMPARE(settings.excelImportTimeoutSeconds(), 120);
}

void AiCommentOptionsTests::sidebarDisplayDefaultsAndPersist()
{
    SettingsManager& settings =
        SettingsManager::instance();
    settings.remove(
        QString::fromUtf8(
            SettingsManager::Keys::SIDEBAR_TOOLTIPS_ENABLED
            )
        );
    settings.remove(
        QString::fromUtf8(
            SettingsManager::Keys::SIDEBAR_MARQUEE_ENABLED
            )
        );

    ActionRegistry defaults;
    defaults.createActions();
    QVERIFY(defaults.showSidebarTooltips);
    QVERIFY(defaults.animateSidebarText);
    QVERIFY(defaults.showSidebarTooltips->isChecked());
    QVERIFY(defaults.animateSidebarText->isChecked());
    QVERIFY(
        !settings.get(
            QString::fromUtf8(
                SettingsManager::Keys::SIDEBAR_TOOLTIPS_ENABLED
                )
            ).isValid()
        );
    QVERIFY(
        !settings.get(
            QString::fromUtf8(
                SettingsManager::Keys::SIDEBAR_MARQUEE_ENABLED
                )
            ).isValid()
        );

    defaults.showSidebarTooltips->setChecked(false);
    defaults.animateSidebarText->setChecked(false);
    QVERIFY(
        !settings.get(
            QString::fromUtf8(
                SettingsManager::Keys::SIDEBAR_TOOLTIPS_ENABLED
                )
            ).toBool()
        );
    QVERIFY(
        !settings.get(
            QString::fromUtf8(
                SettingsManager::Keys::SIDEBAR_MARQUEE_ENABLED
                )
            ).toBool()
        );

    ActionRegistry reloaded;
    reloaded.createActions();
    QVERIFY(!reloaded.showSidebarTooltips->isChecked());
    QVERIFY(!reloaded.animateSidebarText->isChecked());
}

QTEST_MAIN(AiCommentOptionsTests)

#include "ai_comment_options_tests.moc"

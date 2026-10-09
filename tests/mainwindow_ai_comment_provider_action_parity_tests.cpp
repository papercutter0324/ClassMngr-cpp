#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "next/platform/settings_manager_ai_comment_provider_preferences_port.h"

#include <QAction>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include <utility>

namespace
{
class AiCommentProviderRestorer final
{
public:
    AiCommentProviderRestorer(
        MainWindow& window,
        const ::AiCommentProvider originalProvider
        )
        : m_window(window),
          m_originalProvider(originalProvider)
    {
    }

    ~AiCommentProviderRestorer()
    {
        auto* const state = m_window.actions().aiCommentProviderState;
        if (state && state->current() != m_originalProvider)
        {
            QAction* const originalAction =
                state->action(m_originalProvider);
            if (originalAction)
            {
                originalAction->trigger();
            }
        }

        SettingsManager::instance().sync();
    }

    AiCommentProviderRestorer(
        const AiCommentProviderRestorer&
        ) = delete;
    AiCommentProviderRestorer& operator=(
        const AiCommentProviderRestorer&
        ) = delete;

private:
    MainWindow& m_window;
    ::AiCommentProvider m_originalProvider;
};
}

class MainWindowAiCommentProviderActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void providerActionsUpdateProviderAndBatchDialogLabel();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowAiCommentProviderActionParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsDirectory.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowAiCommentProviderActionParityTests::
providerActionsUpdateProviderAndBatchDialogLabel()
{
    using LegacyProvider = ::AiCommentProvider;
    using PersistedProvider =
        ClassMngr::Next::Application::AiCommentProvider;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentProviderPreferencesPort;

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    const SettingsManagerAiCommentProviderPreferencesPort preferences;
    preferences.write(PersistedProvider::ChatGPT);
    SettingsManager::instance().sync();

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    auto* const state = window.actions().aiCommentProviderState;
    QVERIFY(state);
    const LegacyProvider originalProvider = state->current();
    AiCommentProviderRestorer providerRestorer(
        window,
        originalProvider
        );

    QAction* const chatGptAction =
        state->action(LegacyProvider::ChatGPT);
    QAction* const geminiAction =
        state->action(LegacyProvider::Gemini);
    QAction* const claudeAction =
        state->action(LegacyProvider::Claude);
    QAction* const microsoftCopilotAction =
        state->action(LegacyProvider::MicrosoftCopilot);
    QAction* const customWebsiteAction =
        state->action(LegacyProvider::CustomWebsite);
    QVERIFY(chatGptAction);
    QVERIFY(geminiAction);
    QVERIFY(claudeAction);
    QVERIFY(microsoftCopilotAction);
    QVERIFY(customWebsiteAction);
    QCOMPARE(originalProvider, LegacyProvider::ChatGPT);
    QCOMPARE(state->current(), LegacyProvider::ChatGPT);
    QVERIFY(chatGptAction->isCheckable());
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(geminiAction->isCheckable());
    QVERIFY(!geminiAction->isChecked());
    QVERIFY(claudeAction->isCheckable());
    QVERIFY(!claudeAction->isChecked());
    QVERIFY(microsoftCopilotAction->isCheckable());
    QVERIFY(!microsoftCopilotAction->isChecked());
    QVERIFY(customWebsiteAction->isCheckable());
    QVERIFY(!customWebsiteAction->isChecked());
    QCOMPARE(preferences.read(), PersistedProvider::ChatGPT);

    QVERIFY(geminiAction->isEnabled());
    geminiAction->trigger();

    QCOMPARE(state->current(), LegacyProvider::Gemini);
    QVERIFY(geminiAction->isChecked());
    QVERIFY(!chatGptAction->isChecked());
    QVERIFY(!claudeAction->isChecked());
    QVERIFY(!microsoftCopilotAction->isChecked());
    QVERIFY(!customWebsiteAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(preferences.read(), PersistedProvider::Gemini);

    SpeakingEvalReportData report;
    report.englishName = QStringLiteral("Synthetic Student");
    report.grade = 4;
    const QList<SpeakingEvalBatchReportService::StudentReport>
        singleStudentReports{
            {
                report.englishName,
                report,
                0
            }
        };

    SpeakingEvalReportDialog geminiReportDialog(
        singleStudentReports,
        0,
        nullptr,
        true
        );
    QPushButton* const geminiReportCopyOpenButton =
        geminiReportDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalCopyOpenAiPromptButton")
            );
    QVERIFY(geminiReportCopyOpenButton);
    QVERIFY(
        geminiReportCopyOpenButton->text().contains(
            QStringLiteral("Gemini")
            )
        );

    SpeakingEvalAiBatchDialog dialog(
        {
            {
                report.englishName,
                report,
                0
            }
        }
        );

    QPushButton* const copyOpenButton = dialog.findChild<QPushButton*>(
        QStringLiteral("speakingEvalAiBatchCopyOpen")
        );
    QVERIFY(copyOpenButton);
    QVERIFY(
        copyOpenButton->text().contains(QStringLiteral("Gemini"))
        );

    QVERIFY(claudeAction->isEnabled());
    claudeAction->trigger();

    QCOMPARE(state->current(), LegacyProvider::Claude);
    QVERIFY(claudeAction->isChecked());
    QVERIFY(!chatGptAction->isChecked());
    QVERIFY(!geminiAction->isChecked());
    QVERIFY(!microsoftCopilotAction->isChecked());
    QVERIFY(!customWebsiteAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(preferences.read(), PersistedProvider::Claude);

    SpeakingEvalReportDialog claudeReportDialog(
        singleStudentReports,
        0,
        nullptr,
        true
        );
    QPushButton* const claudeReportCopyOpenButton =
        claudeReportDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalCopyOpenAiPromptButton")
            );
    QVERIFY(claudeReportCopyOpenButton);
    QVERIFY(
        claudeReportCopyOpenButton->text().contains(
            QStringLiteral("Claude")
            )
        );

    SpeakingEvalAiBatchDialog claudeDialog(
        {
            {
                report.englishName,
                report,
                0
            }
        }
        );

    QPushButton* const claudeCopyOpenButton =
        claudeDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchCopyOpen")
            );
    QVERIFY(claudeCopyOpenButton);
    QVERIFY(
        claudeCopyOpenButton->text().contains(QStringLiteral("Claude"))
        );

    QVERIFY(microsoftCopilotAction->isEnabled());
    microsoftCopilotAction->trigger();

    QCOMPARE(state->current(), LegacyProvider::MicrosoftCopilot);
    QVERIFY(microsoftCopilotAction->isChecked());
    QVERIFY(!chatGptAction->isChecked());
    QVERIFY(!geminiAction->isChecked());
    QVERIFY(!claudeAction->isChecked());
    QVERIFY(!customWebsiteAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(preferences.read(), PersistedProvider::MicrosoftCopilot);

    SpeakingEvalReportDialog microsoftCopilotReportDialog(
        singleStudentReports,
        0,
        nullptr,
        true
        );
    QPushButton* const microsoftCopilotReportCopyOpenButton =
        microsoftCopilotReportDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalCopyOpenAiPromptButton")
            );
    QVERIFY(microsoftCopilotReportCopyOpenButton);
    QVERIFY(
        microsoftCopilotReportCopyOpenButton->text().contains(
            QStringLiteral("Microsoft Copilot")
            )
        );

    SpeakingEvalAiBatchDialog microsoftCopilotDialog(
        {
            {
                report.englishName,
                report,
                0
            }
        }
        );

    QPushButton* const microsoftCopilotCopyOpenButton =
        microsoftCopilotDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchCopyOpen")
            );
    QVERIFY(microsoftCopilotCopyOpenButton);
    QVERIFY(
        microsoftCopilotCopyOpenButton->text().contains(
            QStringLiteral("Microsoft Copilot")
            )
        );

    QVERIFY(chatGptAction->isEnabled());
    chatGptAction->trigger();

    QCOMPARE(state->current(), LegacyProvider::ChatGPT);
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(!geminiAction->isChecked());
    QVERIFY(!claudeAction->isChecked());
    QVERIFY(!microsoftCopilotAction->isChecked());
    QVERIFY(!customWebsiteAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(preferences.read(), PersistedProvider::ChatGPT);

    SpeakingEvalReportDialog chatGptReportDialog(
        singleStudentReports,
        0,
        nullptr,
        true
        );
    QPushButton* const chatGptReportCopyOpenButton =
        chatGptReportDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalCopyOpenAiPromptButton")
            );
    QVERIFY(chatGptReportCopyOpenButton);
    QVERIFY(
        chatGptReportCopyOpenButton->text().contains(
            QStringLiteral("ChatGPT")
            )
        );

    SpeakingEvalAiBatchDialog chatGptDialog(
        {
            {
                report.englishName,
                report,
                0
            }
        }
        );

    QPushButton* const chatGptCopyOpenButton =
        chatGptDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchCopyOpen")
            );
    QVERIFY(chatGptCopyOpenButton);
    QCOMPARE(
        chatGptCopyOpenButton->text(),
        QStringLiteral("Copy Prompt and Open ChatGPT")
        );
}

QTEST_MAIN(MainWindowAiCommentProviderActionParityTests)

#include "mainwindow_ai_comment_provider_action_parity_tests.moc"

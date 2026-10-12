#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_ai_comment_provider_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAction>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <string>
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
    void failedStandardProviderActionRestoresStateAndAllowsRetry();

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
    QVERIFY(preferences.write(PersistedProvider::ChatGPT));
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

void MainWindowAiCommentProviderActionParityTests::
failedStandardProviderActionRestoresStateAndAllowsRetry()
{
    using LegacyProvider = ::AiCommentProvider;
    using PersistedProvider =
        ClassMngr::Next::Application::AiCommentProvider;
    using ClassMngr::Next::Domain::ErrorCode;
    using ClassMngr::Next::Domain::OperationError;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentProviderPreferencesPort;

    SettingsManager& settings = SettingsManager::instance();
    settings.clear();
    settings.set(
        QString::fromUtf8(OptionKeys::AiCommentProvider),
        static_cast<int>(PersistedProvider::ChatGPT)
        );
    settings.set(QStringLiteral("options/unrelatedValue"), 53);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const auto preferences =
        std::make_shared<SettingsManagerAiCommentProviderPreferencesPort>(
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

    int diagnosticCount = 0;
    OperationError lastDiagnostic;
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.aiCommentProviderPreferencesPort = preferences;
    startupOptions.aiCommentProviderPreferenceErrorHandler =
        [&diagnosticCount, &lastDiagnostic](const OperationError& error)
        {
            ++diagnosticCount;
            lastDiagnostic = error;
        };

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    QCOMPARE(synchronizationAttempts, 0);
    QCOMPARE(diagnosticCount, 0);
    auto* const state = window.actions().aiCommentProviderState;
    QVERIFY(state);
    QCOMPARE(state->current(), LegacyProvider::ChatGPT);
    QAction* const chatGptAction = state->action(LegacyProvider::ChatGPT);
    QAction* const geminiAction = state->action(LegacyProvider::Gemini);
    QAction* const claudeAction = state->action(LegacyProvider::Claude);
    QAction* const microsoftCopilotAction =
        state->action(LegacyProvider::MicrosoftCopilot);
    QAction* const customWebsiteAction =
        state->action(LegacyProvider::CustomWebsite);
    QVERIFY(chatGptAction);
    QVERIFY(geminiAction);
    QVERIFY(claudeAction);
    QVERIFY(microsoftCopilotAction);
    QVERIFY(customWebsiteAction);

    const QVariant storedProviderBeforeFailure = settings.get(
        QString::fromUtf8(OptionKeys::AiCommentProvider)
        );
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    const QVariant unrelatedValueBeforeFailure = settings.get(unrelatedKey);

    SpeakingEvalReportData report;
    report.englishName = QStringLiteral("Synthetic Student");
    report.grade = 4;
    const QList<SpeakingEvalBatchReportService::StudentReport> reports{
        {
            report.englishName,
            report,
            0
        }
    };
    const auto verifyDialogLabels = [&reports](const QString& providerLabel)
    {
        SpeakingEvalReportDialog reportDialog(reports, 0, nullptr, true);
        QPushButton* const reportButton = reportDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalCopyOpenAiPromptButton")
            );
        QVERIFY(reportButton);
        QVERIFY(reportButton->text().contains(providerLabel));

        SpeakingEvalAiBatchDialog batchDialog(reports);
        QPushButton* const batchButton = batchDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchCopyOpen")
            );
        QVERIFY(batchButton);
        QVERIFY(batchButton->text().contains(providerLabel));
    };

    QVERIFY(geminiAction->isEnabled());
    geminiAction->trigger();

    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(diagnosticCount, 1);
    QCOMPARE(lastDiagnostic.code, ErrorCode::Technical);
    QVERIFY(lastDiagnostic.recoverable);
    QVERIFY(
        lastDiagnostic.message.find("AccessError") != std::string::npos
        );
    QCOMPARE(state->current(), LegacyProvider::ChatGPT);
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(!geminiAction->isChecked());
    QVERIFY(!claudeAction->isChecked());
    QVERIFY(!microsoftCopilotAction->isChecked());
    QVERIFY(!customWebsiteAction->isChecked());
    QCOMPARE(
        settings.get(QString::fromUtf8(OptionKeys::AiCommentProvider)),
        storedProviderBeforeFailure
        );
    QCOMPARE(preferences->read(), PersistedProvider::ChatGPT);
    QCOMPARE(settings.get(unrelatedKey), unrelatedValueBeforeFailure);
    verifyDialogLabels(QStringLiteral("ChatGPT"));

    injectAccessError = false;
    geminiAction->trigger();

    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(diagnosticCount, 1);
    QCOMPARE(state->current(), LegacyProvider::Gemini);
    QVERIFY(geminiAction->isChecked());
    QVERIFY(!chatGptAction->isChecked());
    QCOMPARE(
        settings.get(QString::fromUtf8(OptionKeys::AiCommentProvider)).toInt(),
        static_cast<int>(PersistedProvider::Gemini)
        );
    QCOMPARE(preferences->read(), PersistedProvider::Gemini);
    QCOMPARE(settings.get(unrelatedKey), unrelatedValueBeforeFailure);
    verifyDialogLabels(QStringLiteral("Gemini"));

    injectAccessError = true;
    state->set(LegacyProvider::Claude);

    QCOMPARE(synchronizationAttempts, 3);
    QCOMPARE(diagnosticCount, 2);
    QCOMPARE(lastDiagnostic.code, ErrorCode::Technical);
    QCOMPARE(state->current(), LegacyProvider::Claude);
    QVERIFY(claudeAction->isChecked());
    QVERIFY(!geminiAction->isChecked());
    QCOMPARE(
        settings.get(QString::fromUtf8(OptionKeys::AiCommentProvider)).toInt(),
        static_cast<int>(PersistedProvider::Gemini)
        );
    QCOMPARE(preferences->read(), PersistedProvider::Gemini);
}

QTEST_MAIN(MainWindowAiCommentProviderActionParityTests)

#include "mainwindow_ai_comment_provider_action_parity_tests.moc"

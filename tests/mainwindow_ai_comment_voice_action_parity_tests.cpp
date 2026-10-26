#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_ai_comment_voice_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAction>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <string>
#include <utility>

namespace
{
class AiCommentVoiceRestorer final
{
public:
    AiCommentVoiceRestorer(
        MainWindow& window,
        const ::AiCommentVoice originalVoice
        )
        : m_window(window),
          m_originalVoice(originalVoice)
    {
    }

    ~AiCommentVoiceRestorer()
    {
        auto* const state = m_window.actions().aiCommentVoiceState;
        if (!state || state->current() == m_originalVoice)
        {
            return;
        }

        QAction* const originalAction =
            state->action(m_originalVoice);
        if (originalAction)
        {
            originalAction->trigger();
            SettingsManager::instance().sync();
        }
    }

    AiCommentVoiceRestorer(const AiCommentVoiceRestorer&) = delete;
    AiCommentVoiceRestorer& operator=(
        const AiCommentVoiceRestorer&
        ) = delete;

private:
    MainWindow& m_window;
    ::AiCommentVoice m_originalVoice;
};

QString promptFromBatchDialog(const SpeakingEvalReportData& report)
{
    SpeakingEvalAiBatchDialog dialog(
        {
            {
                report.englishName,
                report,
                0
            }
        }
        );

    auto* const createPromptButton = dialog.findChild<QPushButton*>(
        QStringLiteral("speakingEvalAiBatchCreatePrompt")
        );
    auto* const promptEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalAiBatchPrompt")
        );
    if (!createPromptButton || !promptEdit || !createPromptButton->isEnabled())
    {
        return {};
    }

    createPromptButton->click();
    return promptEdit->toPlainText();
}
}

class MainWindowAiCommentVoiceActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void thirdPersonActionControlsBatchPromptVoice();
    void failedThirdPersonActionPreservesPromptVoiceAndAllowsRetry();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowAiCommentVoiceActionParityTests::initTestCase()
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

void MainWindowAiCommentVoiceActionParityTests::
thirdPersonActionControlsBatchPromptVoice()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    auto* const state = window.actions().aiCommentVoiceState;
    QVERIFY(state);
    const ::AiCommentVoice originalVoice = state->current();
    AiCommentVoiceRestorer voiceRestorer(window, originalVoice);

    QAction* const directAction =
        state->action(::AiCommentVoice::DirectToStudent);
    QAction* const thirdPersonAction =
        state->action(::AiCommentVoice::ThirdPerson);
    QVERIFY(directAction);
    QVERIFY(thirdPersonAction);

    const ClassMngr::Next::Platform::
        SettingsManagerAiCommentVoicePreferencesPort preferences;
    QCOMPARE(state->current(), ::AiCommentVoice::DirectToStudent);
    QVERIFY(directAction->isCheckable());
    QVERIFY(directAction->isChecked());
    QVERIFY(thirdPersonAction->isCheckable());
    QVERIFY(!thirdPersonAction->isChecked());
    QCOMPARE(
        preferences.read(),
        ClassMngr::Next::Application::AiCommentVoice::DirectToStudent
        );

    QVERIFY(thirdPersonAction->isEnabled());
    thirdPersonAction->trigger();

    QCOMPARE(state->current(), ::AiCommentVoice::ThirdPerson);
    QVERIFY(thirdPersonAction->isChecked());
    QVERIFY(!directAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(
        preferences.read(),
        ClassMngr::Next::Application::AiCommentVoice::ThirdPerson
        );

    SpeakingEvalReportData report;
    report.englishName = QStringLiteral("Synthetic Student");
    report.grade = 4;
    report.comments.clear();
    report.notes = QStringLiteral(
        "[Did Well]\nClear pronunciation\n"
        "[Needs Improvement]\nAdd supporting details"
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

    auto* const selection = dialog.findChild<QTableWidget*>(
        QStringLiteral("speakingEvalAiBatchSelectionTable")
        );
    auto* const createPromptButton = dialog.findChild<QPushButton*>(
        QStringLiteral("speakingEvalAiBatchCreatePrompt")
        );
    auto* const promptEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalAiBatchPrompt")
        );
    QVERIFY(selection);
    QVERIFY(createPromptButton);
    QVERIFY(promptEdit);
    QCOMPARE(selection->rowCount(), 1);
    QVERIFY(selection->item(0, 0));
    QVERIFY(selection->item(0, 0)->flags() & Qt::ItemIsEnabled);
    QCOMPARE(selection->item(0, 0)->checkState(), Qt::Checked);
    QVERIFY(selection->item(0, 2));
    QCOMPARE(selection->item(0, 2)->text(), QStringLiteral("Ready"));
    QVERIFY(createPromptButton->isEnabled());

    createPromptButton->click();

    const QString prompt = promptEdit->toPlainText();
    QVERIFY(!prompt.isEmpty());
    QVERIFY(
        prompt.contains(
            QStringLiteral("Write for a parent or guardian")
            )
        );
    QVERIFY(
        prompt.contains(
            QStringLiteral(
                "use they/their rather than guessing gender."
                )
            )
        );

    QVERIFY(directAction->isEnabled());
    directAction->trigger();

    QCOMPARE(state->current(), ::AiCommentVoice::DirectToStudent);
    QVERIFY(directAction->isChecked());
    QVERIFY(!thirdPersonAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(
        preferences.read(),
        ClassMngr::Next::Application::AiCommentVoice::DirectToStudent
        );

    SpeakingEvalAiBatchDialog directDialog(
        {
            {
                report.englishName,
                report,
                0
            }
        }
        );

    auto* const directCreatePromptButton = directDialog.findChild<QPushButton*>(
        QStringLiteral("speakingEvalAiBatchCreatePrompt")
        );
    auto* const directPromptEdit = directDialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalAiBatchPrompt")
        );
    QVERIFY(directCreatePromptButton);
    QVERIFY(directPromptEdit);
    QVERIFY(directCreatePromptButton->isEnabled());

    directCreatePromptButton->click();

    const QString directPrompt = directPromptEdit->toPlainText();
    QVERIFY(!directPrompt.isEmpty());
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "Address the student directly as \"you\" and use STD_NAME naturally."
                )
            )
        );
    QVERIFY(
        !directPrompt.contains(
            QStringLiteral("Write for a parent or guardian")
            )
        );
    QVERIFY(
        !directPrompt.contains(
            QStringLiteral(
                "use they/their rather than guessing gender."
                )
            )
        );
}

void MainWindowAiCommentVoiceActionParityTests::
failedThirdPersonActionPreservesPromptVoiceAndAllowsRetry()
{
    using namespace ClassMngr::Next::Application;
    using ClassMngr::Next::Domain::ErrorCode;
    using ClassMngr::Next::Domain::OperationError;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentVoicePreferencesPort;

    SettingsManager& settings = SettingsManager::instance();
    settings.clear();
    settings.sync();

    const QString preferenceKey = QString::fromUtf8(
        OptionKeys::AiCommentVoice
        );
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.set(preferenceKey, 0);
    settings.set(unrelatedKey, 53);
    settings.sync();

    bool injectAccessError = false;
    int synchronizationAttempts = 0;
    const auto preferences = std::make_shared<
        SettingsManagerAiCommentVoicePreferencesPort
        >(
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
    startupOptions.aiCommentVoicePreferencesPort = preferences;
    startupOptions.aiCommentVoicePreferenceErrorHandler =
        [&diagnosticCount, &lastDiagnostic](const OperationError& error)
        {
            ++diagnosticCount;
            lastDiagnostic = error;
        };

    MainWindow window(
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );

    auto* const state = window.actions().aiCommentVoiceState;
    QVERIFY(state);
    QCOMPARE(state->current(), ::AiCommentVoice::DirectToStudent);
    AiCommentVoiceRestorer voiceRestorer(
        window,
        ::AiCommentVoice::DirectToStudent
        );

    QAction* const directAction =
        state->action(::AiCommentVoice::DirectToStudent);
    QAction* const thirdPersonAction =
        state->action(::AiCommentVoice::ThirdPerson);
    QVERIFY(directAction);
    QVERIFY(thirdPersonAction);
    QVERIFY(directAction->isChecked());
    QVERIFY(!thirdPersonAction->isChecked());

    const QVariant storedPreferenceBeforeFailure = settings.get(preferenceKey);
    const QString reportFile = m_settingsDirectory.filePath(
        QStringLiteral("PaperCloud/ClassMngr.ini")
        );
    QSettings persistedSettings(
        reportFile,
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(preferenceKey).toInt(), 0);

    SpeakingEvalReportData report;
    report.englishName = QStringLiteral("Synthetic Student");
    report.grade = 4;
    report.comments.clear();
    report.notes = QStringLiteral(
        "[Did Well]\nClear pronunciation\n"
        "[Needs Improvement]\nAdd supporting details"
        );

    QCOMPARE(synchronizationAttempts, 0);
    QCOMPARE(diagnosticCount, 0);
    injectAccessError = true;
    thirdPersonAction->trigger();
    injectAccessError = false;

    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(diagnosticCount, 1);
    QCOMPARE(lastDiagnostic.code, ErrorCode::Technical);
    QVERIFY(lastDiagnostic.recoverable);
    QVERIFY(
        lastDiagnostic.message.find("AccessError") != std::string::npos
        );
    QCOMPARE(state->current(), ::AiCommentVoice::DirectToStudent);
    QVERIFY(directAction->isChecked());
    QVERIFY(!thirdPersonAction->isChecked());
    QCOMPARE(settings.get(preferenceKey).toInt(), 0);
    QCOMPARE(settings.get(preferenceKey), storedPreferenceBeforeFailure);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 53);

    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(preferenceKey).toInt(), 0);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 53);

    const QString promptAfterFailure = promptFromBatchDialog(report);
    QVERIFY(!promptAfterFailure.isEmpty());
    QVERIFY(
        promptAfterFailure.contains(
            QStringLiteral(
                "Address the student directly as \"you\" and use STD_NAME naturally."
                )
            )
        );
    QVERIFY(
        !promptAfterFailure.contains(
            QStringLiteral("Write for a parent or guardian")
            )
        );

    injectAccessError = false;
    thirdPersonAction->trigger();

    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(diagnosticCount, 1);
    QCOMPARE(state->current(), ::AiCommentVoice::ThirdPerson);
    QVERIFY(!directAction->isChecked());
    QVERIFY(thirdPersonAction->isChecked());
    QCOMPARE(settings.get(preferenceKey).toInt(), 1);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 53);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(preferenceKey).toInt(), 1);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 53);

    const QString promptAfterRetry = promptFromBatchDialog(report);
    QVERIFY(!promptAfterRetry.isEmpty());
    QVERIFY(
        promptAfterRetry.contains(
            QStringLiteral("Write for a parent or guardian")
            )
        );
    QVERIFY(
        promptAfterRetry.contains(
            QStringLiteral(
                "use they/their rather than guessing gender."
                )
            )
        );
}

QTEST_MAIN(MainWindowAiCommentVoiceActionParityTests)

#include "mainwindow_ai_comment_voice_action_parity_tests.moc"

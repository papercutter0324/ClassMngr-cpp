#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "next/platform/settings_manager_ai_comment_voice_preferences_port.h"

#include <QAction>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

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
}

class MainWindowAiCommentVoiceActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void thirdPersonActionControlsBatchPromptVoice();

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
        [](const QString&) {},
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
}

QTEST_MAIN(MainWindowAiCommentVoiceActionParityTests)

#include "mainwindow_ai_comment_voice_action_parity_tests.moc"

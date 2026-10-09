#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
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
    void geminiActionUpdatesProviderAndBatchDialogLabel();

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
geminiActionUpdatesProviderAndBatchDialogLabel()
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
    QVERIFY(chatGptAction);
    QVERIFY(geminiAction);
    QCOMPARE(originalProvider, LegacyProvider::ChatGPT);
    QCOMPARE(state->current(), LegacyProvider::ChatGPT);
    QVERIFY(chatGptAction->isCheckable());
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(geminiAction->isCheckable());
    QVERIFY(!geminiAction->isChecked());
    QCOMPARE(preferences.read(), PersistedProvider::ChatGPT);

    QVERIFY(geminiAction->isEnabled());
    geminiAction->trigger();

    QCOMPARE(state->current(), LegacyProvider::Gemini);
    QVERIFY(geminiAction->isChecked());
    QVERIFY(!chatGptAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(preferences.read(), PersistedProvider::Gemini);

    SpeakingEvalReportData report;
    report.englishName = QStringLiteral("Synthetic Student");
    report.grade = 4;

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
}

QTEST_MAIN(MainWindowAiCommentProviderActionParityTests)

#include "mainwindow_ai_comment_provider_action_parity_tests.moc"

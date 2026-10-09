#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "next/platform/settings_manager_ai_comment_custom_website_port.h"
#include "next/platform/settings_manager_ai_comment_provider_preferences_port.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <string>
#include <utility>

namespace
{
class CustomWebsiteSettingsRestorer final
{
public:
    CustomWebsiteSettingsRestorer(
        MainWindow& window,
        const ::AiCommentProvider originalProvider,
        std::string originalUrl
        )
        : m_window(window),
          m_originalProvider(originalProvider),
          m_originalUrl(std::move(originalUrl))
    {
    }

    ~CustomWebsiteSettingsRestorer()
    {
        if (auto* const state = m_window.actions().aiCommentProviderState)
        {
            state->set(m_originalProvider);
        }

        ClassMngr::Next::Platform::
            SettingsManagerAiCommentCustomWebsitePort().write(
                m_originalUrl
                );
        SettingsManager::instance().sync();
    }

    CustomWebsiteSettingsRestorer(
        const CustomWebsiteSettingsRestorer&
        ) = delete;
    CustomWebsiteSettingsRestorer& operator=(
        const CustomWebsiteSettingsRestorer&
        ) = delete;

private:
    MainWindow& m_window;
    ::AiCommentProvider m_originalProvider;
    std::string m_originalUrl;
};

void rejectUnexpectedModal()
{
    QWidget* modal = QApplication::activeModalWidget();
    if (!modal)
    {
        for (QWidget* const topLevel : QApplication::topLevelWidgets())
        {
            if (topLevel && topLevel->isVisible()
                && qobject_cast<QDialog*>(topLevel))
            {
                modal = topLevel;
                break;
            }
        }
    }

    if (auto* const dialog = qobject_cast<QDialog*>(modal))
    {
        dialog->reject();
    }
    else if (modal)
    {
        modal->close();
    }
}
}

class MainWindowCustomWebsiteAiCommentProviderActionParityTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void customWebsiteActionCapturesAndPersistsEnteredUrl();
    void customWebsiteActionCancellationKeepsPreviousSettings();
    void customWebsiteActionInvalidUrlKeepsPreviousSettingsAndReportsWarning();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowCustomWebsiteAiCommentProviderActionParityTests::
initTestCase()
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

void MainWindowCustomWebsiteAiCommentProviderActionParityTests::
customWebsiteActionCapturesAndPersistsEnteredUrl()
{
    using PersistedProvider =
        ClassMngr::Next::Application::AiCommentProvider;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentCustomWebsitePort;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentProviderPreferencesPort;

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    const std::string originalUrl =
        "https://original.example.test/";
    const QString enteredUrl =
        QStringLiteral("https://example.test/");
    const SettingsManagerAiCommentProviderPreferencesPort providerPreferences;
    const SettingsManagerAiCommentCustomWebsitePort customWebsitePort;
    providerPreferences.write(PersistedProvider::ChatGPT);
    customWebsitePort.write(originalUrl);
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
    const ::AiCommentProvider originalProvider = state->current();
    CustomWebsiteSettingsRestorer settingsRestorer(
        window,
        originalProvider,
        customWebsitePort.read()
        );

    QAction* const chatGptAction =
        state->action(::AiCommentProvider::ChatGPT);
    QAction* const customWebsiteAction =
        state->action(::AiCommentProvider::CustomWebsite);
    QVERIFY(chatGptAction);
    QVERIFY(customWebsiteAction);
    QCOMPARE(originalProvider, ::AiCommentProvider::ChatGPT);
    QCOMPARE(state->current(), ::AiCommentProvider::ChatGPT);
    QVERIFY(chatGptAction->isCheckable());
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(customWebsiteAction->isCheckable());
    QVERIFY(!customWebsiteAction->isChecked());
    QCOMPARE(
        providerPreferences.read(),
        PersistedProvider::ChatGPT
        );
    QCOMPARE(customWebsitePort.read(), originalUrl);
    QVERIFY(customWebsiteAction->isEnabled());

    bool modalObserved = false;
    bool unexpectedModalObserved = false;
    QPointer<QInputDialog> observedInputDialog;
    QTimer::singleShot(
        0,
        &window,
        [&modalObserved, &unexpectedModalObserved, &observedInputDialog,
         &enteredUrl]()
        {
            QWidget* const activeModal = QApplication::activeModalWidget();
            auto* const inputDialog = qobject_cast<QInputDialog*>(activeModal);
            if (!inputDialog)
            {
                unexpectedModalObserved = activeModal != nullptr;
                rejectUnexpectedModal();
                return;
            }

            modalObserved = true;
            observedInputDialog = inputDialog;
            inputDialog->setTextValue(enteredUrl);
            inputDialog->accept();
        }
        );

    customWebsiteAction->trigger();

    QVERIFY(modalObserved);
    QVERIFY(!unexpectedModalObserved);
    QVERIFY(observedInputDialog.isNull());
    QCOMPARE(state->current(), ::AiCommentProvider::CustomWebsite);
    QVERIFY(customWebsiteAction->isChecked());
    QVERIFY(!chatGptAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(
        providerPreferences.read(),
        PersistedProvider::CustomWebsite
        );
    const std::string storedUrl = customWebsitePort.read();
    QCOMPARE(
        QString::fromUtf8(
            storedUrl.data(),
            static_cast<qsizetype>(storedUrl.size())
            ),
        enteredUrl
        );

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

    SpeakingEvalReportDialog reportDialog(
        singleStudentReports,
        0,
        nullptr,
        true
        );
    QPushButton* const reportCopyOpenButton =
        reportDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalCopyOpenAiPromptButton")
            );
    QVERIFY(reportCopyOpenButton);
    QVERIFY(
        reportCopyOpenButton->text().contains(
            QStringLiteral("Custom AI Website")
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
        copyOpenButton->text().contains(
            QStringLiteral("Custom AI Website")
            )
        );
}

void MainWindowCustomWebsiteAiCommentProviderActionParityTests::
customWebsiteActionCancellationKeepsPreviousSettings()
{
    using PersistedProvider =
        ClassMngr::Next::Application::AiCommentProvider;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentCustomWebsitePort;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentProviderPreferencesPort;

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    const std::string originalUrl =
        "https://original.example.test/";
    const QString enteredUrl =
        QStringLiteral("https://cancelled.example.test/");
    const SettingsManagerAiCommentProviderPreferencesPort providerPreferences;
    const SettingsManagerAiCommentCustomWebsitePort customWebsitePort;
    providerPreferences.write(PersistedProvider::ChatGPT);
    customWebsitePort.write(originalUrl);
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

    CustomWebsiteSettingsRestorer settingsRestorer(
        window,
        ::AiCommentProvider::ChatGPT,
        originalUrl
        );
    auto* const state = window.actions().aiCommentProviderState;
    QVERIFY(state);

    QAction* const chatGptAction =
        state->action(::AiCommentProvider::ChatGPT);
    QAction* const customWebsiteAction =
        state->action(::AiCommentProvider::CustomWebsite);
    QVERIFY(chatGptAction);
    QVERIFY(customWebsiteAction);
    QCOMPARE(state->current(), ::AiCommentProvider::ChatGPT);
    QVERIFY(chatGptAction->isCheckable());
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(customWebsiteAction->isCheckable());
    QVERIFY(!customWebsiteAction->isChecked());
    QCOMPARE(
        providerPreferences.read(),
        PersistedProvider::ChatGPT
        );
    QCOMPARE(customWebsitePort.read(), originalUrl);
    QVERIFY(customWebsiteAction->isEnabled());

    bool modalObserved = false;
    bool unexpectedModalObserved = false;
    QPointer<QInputDialog> observedInputDialog;
    QTimer::singleShot(
        0,
        &window,
        [&modalObserved, &unexpectedModalObserved, &observedInputDialog,
         &enteredUrl]()
        {
            QWidget* const activeModal = QApplication::activeModalWidget();
            auto* const inputDialog = qobject_cast<QInputDialog*>(activeModal);
            if (!inputDialog)
            {
                unexpectedModalObserved = activeModal != nullptr;
                rejectUnexpectedModal();
                return;
            }

            modalObserved = true;
            observedInputDialog = inputDialog;
            inputDialog->setTextValue(enteredUrl);
            inputDialog->reject();
        }
        );

    customWebsiteAction->trigger();

    QVERIFY(modalObserved);
    QVERIFY(!unexpectedModalObserved);
    QVERIFY(observedInputDialog.isNull());
    QCOMPARE(state->current(), ::AiCommentProvider::ChatGPT);
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(!customWebsiteAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(
        providerPreferences.read(),
        PersistedProvider::ChatGPT
        );
    QCOMPARE(customWebsitePort.read(), originalUrl);
}

void MainWindowCustomWebsiteAiCommentProviderActionParityTests::
customWebsiteActionInvalidUrlKeepsPreviousSettingsAndReportsWarning()
{
    using PersistedProvider =
        ClassMngr::Next::Application::AiCommentProvider;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentCustomWebsitePort;
    using ClassMngr::Next::Platform::
        SettingsManagerAiCommentProviderPreferencesPort;

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    const std::string originalUrl =
        "https://original.example.test/";
    const QString enteredUrl =
        QStringLiteral("http://invalid.example.test/");
    const SettingsManagerAiCommentProviderPreferencesPort providerPreferences;
    const SettingsManagerAiCommentCustomWebsitePort customWebsitePort;
    providerPreferences.write(PersistedProvider::ChatGPT);
    customWebsitePort.write(originalUrl);
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

    CustomWebsiteSettingsRestorer settingsRestorer(
        window,
        ::AiCommentProvider::ChatGPT,
        originalUrl
        );
    auto* const state = window.actions().aiCommentProviderState;
    QVERIFY(state);

    QAction* const chatGptAction =
        state->action(::AiCommentProvider::ChatGPT);
    QAction* const customWebsiteAction =
        state->action(::AiCommentProvider::CustomWebsite);
    QVERIFY(chatGptAction);
    QVERIFY(customWebsiteAction);
    QCOMPARE(state->current(), ::AiCommentProvider::ChatGPT);
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(!customWebsiteAction->isChecked());
    QCOMPARE(providerPreferences.read(), PersistedProvider::ChatGPT);
    QCOMPARE(customWebsitePort.read(), originalUrl);

    bool inputDialogObserved = false;
    bool unexpectedInputModalObserved = false;
    bool warningObserved = false;
    bool unexpectedWarningModalObserved = false;
    QString warningTitle;
    QString warningText;
    QPointer<QInputDialog> observedInputDialog;
    QPointer<QMessageBox> observedWarning;

    QTimer modalSafetyTimer(&window);
    modalSafetyTimer.setSingleShot(true);
    connect(
        &modalSafetyTimer,
        &QTimer::timeout,
        &window,
        []()
        {
            rejectUnexpectedModal();
        }
        );
    modalSafetyTimer.start(2000);

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QWidget* const activeModal = QApplication::activeModalWidget();
            auto* const inputDialog =
                qobject_cast<QInputDialog*>(activeModal);
            if (!inputDialog)
            {
                unexpectedInputModalObserved = activeModal != nullptr;
                rejectUnexpectedModal();
                return;
            }

            inputDialogObserved = true;
            observedInputDialog = inputDialog;
            inputDialog->setTextValue(enteredUrl);

            QTimer::singleShot(
                0,
                &window,
                [&]()
                {
                    QWidget* const warningModal =
                        QApplication::activeModalWidget();
                    auto* const warning =
                        qobject_cast<QMessageBox*>(warningModal);
                    if (!warning)
                    {
                        unexpectedWarningModalObserved =
                            warningModal != nullptr;
                        rejectUnexpectedModal();
                        return;
                    }

                    warningObserved = true;
                    observedWarning = warning;
                    warningTitle = warning->windowTitle();
                    warningText = warning->text();
                    warning->accept();
                }
                );
            inputDialog->accept();
        }
        );

    customWebsiteAction->trigger();
    modalSafetyTimer.stop();

    QVERIFY(inputDialogObserved);
    QVERIFY(!unexpectedInputModalObserved);
    QVERIFY(observedInputDialog.isNull());
    QVERIFY(warningObserved);
    QVERIFY(!unexpectedWarningModalObserved);
    QVERIFY(observedWarning.isNull());
    QCOMPARE(warningTitle, QStringLiteral("Invalid AI Website"));
    QCOMPARE(
        warningText,
        QStringLiteral("Enter a valid HTTPS website URL.")
        );

    QCOMPARE(state->current(), ::AiCommentProvider::ChatGPT);
    QVERIFY(chatGptAction->isChecked());
    QVERIFY(!customWebsiteAction->isChecked());
    SettingsManager::instance().sync();
    QCOMPARE(providerPreferences.read(), PersistedProvider::ChatGPT);
    QCOMPARE(customWebsitePort.read(), originalUrl);
}

QTEST_MAIN(MainWindowCustomWebsiteAiCommentProviderActionParityTests)

#include "mainwindow_custom_website_ai_comment_provider_action_parity_tests.moc"

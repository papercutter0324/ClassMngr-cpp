#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "ui/shared/dialogs/about_dialog.h"

#include <QAction>
#include <QApplication>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <utility>

class MainWindowAboutActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void aboutActionShowsModalAboutDialogAndReturnsToMainWindow();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowAboutActionParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowAboutActionParityTests::
aboutActionShowsModalAboutDialogAndReturnsToMainWindow()
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
    window.show();
    QApplication::processEvents();
    QVERIFY(window.isVisible());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QAction* const aboutAction = window.actions().about;
    QVERIFY(aboutAction);
    QVERIFY(aboutAction->isEnabled());

    bool dialogObserved = false;
    bool dialogWasVisible = false;
    bool dialogWasModal = false;
    bool dialogWasParentedToMainWindow = false;
    bool dialogObservationMismatch = false;

    QTimer::singleShot(
        0,
        &window,
        [&]()
        {
            QWidget* const activeModalWidget =
                QApplication::activeModalWidget();
            auto* const dialog = qobject_cast<AboutDialog*>(
                activeModalWidget
                );
            if (!dialog)
            {
                dialogObservationMismatch = true;
                AboutDialog* const childDialog =
                    window.findChild<AboutDialog*>();
                if (activeModalWidget)
                {
                    activeModalWidget->close();
                }
                if (childDialog)
                {
                    childDialog->close();
                }
                return;
            }

            dialogObserved = true;
            dialogWasVisible = dialog->isVisible();
            dialogWasModal = dialog->isModal();
            dialogWasParentedToMainWindow =
                dialog->parentWidget() == &window;
            dialog->close();
        }
        );

    aboutAction->trigger();

    QVERIFY(!dialogObservationMismatch);
    QVERIFY(dialogObserved);
    QVERIFY(dialogWasVisible);
    QVERIFY(dialogWasModal);
    QVERIFY(dialogWasParentedToMainWindow);
    QVERIFY(window.isVisible());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

QTEST_MAIN(MainWindowAboutActionParityTests)

#include "mainwindow_about_action_parity_tests.moc"

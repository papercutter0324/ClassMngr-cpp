#include "app/controllers/update_controller.h"
#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "core/updater/update_configuration.h"
#include "core/updater/update_service.h"
#include "ui/shared/dialogs/update_dialog.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>

#include <memory>
#include <utility>

class MainWindowCheckForUpdatesActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void checkForUpdatesActionShowsManualUpdateDialog();

private:
    std::unique_ptr<QTemporaryDir> m_settingsDirectory;
};

void MainWindowCheckForUpdatesActionParityTests::initTestCase()
{
    const QString isolatedTempRoot =
        QDir::cleanPath(qEnvironmentVariable("TMP"));
    QVERIFY(!isolatedTempRoot.isEmpty());
    QCOMPARE(
        QDir::cleanPath(qEnvironmentVariable("TEMP")),
        isolatedTempRoot
        );
    QCOMPARE(
        QDir::cleanPath(qEnvironmentVariable("TMPDIR")),
        isolatedTempRoot
        );
    QCOMPARE(
        QDir::cleanPath(
            QStandardPaths::writableLocation(
                QStandardPaths::TempLocation
                )
            ),
        isolatedTempRoot
        );
    QVERIFY(QDir().mkpath(isolatedTempRoot));
    m_settingsDirectory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settingsDirectory->isValid());

    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory->path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowCheckForUpdatesActionParityTests::
checkForUpdatesActionShowsManualUpdateDialog()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    UpdateService updateService(UpdateConfiguration{});
    UpdateController updateController(&updateService);
    updateController.setStartupComplete();
    QSignalSpy checkFailedSpy(
        &updateService,
        &UpdateService::checkFailed
        );
    QVERIFY(checkFailedSpy.isValid());

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        &updateController,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();
    QVERIFY(window.isVisible());

    QAction* const checkForUpdatesAction =
        window.actions().checkForUpdates;
    QVERIFY(checkForUpdatesAction);
    QVERIFY(checkForUpdatesAction->isEnabled());

    checkForUpdatesAction->trigger();

    QCOMPARE(checkFailedSpy.count(), 1);
    QCOMPARE(
        checkFailedSpy.takeFirst().at(0).toString(),
        QStringLiteral("GitHub releases API URL is not configured.")
        );

    auto* const dialog = window.findChild<UpdateDialog*>();
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    QCOMPARE(dialog->parentWidget(), static_cast<QWidget*>(&window));
    QVERIFY(!dialog->property("automaticUpdatePrompt").toBool());
    QVERIFY(updateController.hasVisibleDialog());
    QVERIFY(window.isVisible());

    QLabel* const titleLabel =
        dialog->findChild<QLabel*>(QStringLiteral("programUpdateTitle"));
    QVERIFY(titleLabel);
    QCOMPARE(titleLabel->text(), QStringLiteral("Update Check Failed"));

    QLabel* const detailsLabel =
        dialog->findChild<QLabel*>(QStringLiteral("programUpdateDetails"));
    QVERIFY(detailsLabel);
    QCOMPARE(
        detailsLabel->text(),
        QStringLiteral("GitHub releases API URL is not configured.")
        );

    QPushButton* const retryButton =
        dialog->findChild<QPushButton*>(QStringLiteral("updateCheckButton"));
    QVERIFY(retryButton);
    QVERIFY(retryButton->isVisible());
    QVERIFY(retryButton->isEnabled());
    QCOMPARE(retryButton->text(), QStringLiteral("Try Again"));
}

QTEST_MAIN(MainWindowCheckForUpdatesActionParityTests)

#include "mainwindow_check_for_updates_action_parity_tests.moc"

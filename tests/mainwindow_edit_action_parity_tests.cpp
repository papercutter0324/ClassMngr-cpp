#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/my_info/data/personal_details_repository.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "ui/shared/pages/pagemanager.h"

#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QLineEdit>
#include <QList>
#include <QTemporaryDir>
#include <QtTest>

#include <utility>

namespace
{
QLineEdit* personalNameEditor(PersonalDetailsPage* page)
{
    if (!page)
    {
        return nullptr;
    }

    const QList<QLineEdit*> editors = page->findChildren<QLineEdit*>();
    return editors.isEmpty() ? nullptr : editors.constFirst();
}
}

class MainWindowEditActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void undoAndRedoActionsRestoreAndReapplyPersonalNameInFocusedLineEdit();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowEditActionParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
    qRegisterMetaType<NavigationData>();
}

void MainWindowEditActionParityTests::
undoAndRedoActionsRestoreAndReapplyPersonalNameInFocusedLineEdit()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("undo-workspace.tps"))
        ).absoluteFilePath();
    const QString baselineName =
        QStringLiteral("F450 persisted personal name");

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    PersonalDetails baselineDetails;
    baselineDetails.name = baselineName;
    QVERIFY(
        PersonalDetailsRepository(seedServices.settingsService())
            .save(baselineDetails)
        );
    seedServices.closeDatabase();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

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

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Details);
    QApplication::processEvents();
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    pages->setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);
    QCOMPARE(nameEditor->text(), baselineName);

    nameEditor->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    QVERIFY(nameEditor->hasFocus());
    QCOMPARE(QApplication::focusWidget(), nameEditor);

    const QString draftName = QStringLiteral("F450 Undo draft");
    QVERIFY(draftName != baselineName);
    QTest::keyClick(nameEditor, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClicks(nameEditor, draftName);
    QCOMPARE(nameEditor->text(), draftName);

    QAction* const undoAction = window.actions().undo;
    QVERIFY(undoAction);
    QVERIFY(undoAction->isEnabled());
    undoAction->trigger();

    QCOMPARE(nameEditor->text(), baselineName);
    QVERIFY(nameEditor->hasFocus());
    QCOMPARE(QApplication::focusWidget(), nameEditor);

    QAction* const redoAction = window.actions().redo;
    QVERIFY(redoAction);
    QVERIFY(redoAction->isEnabled());
    redoAction->trigger();

    QCOMPARE(nameEditor->text(), draftName);
    QVERIFY(nameEditor->hasFocus());
    QCOMPARE(QApplication::focusWidget(), nameEditor);
}

QTEST_MAIN(MainWindowEditActionParityTests)

#include "mainwindow_edit_action_parity_tests.moc"

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
#include <QClipboard>
#include <QFileInfo>
#include <QLineEdit>
#include <QList>
#include <QMimeData>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

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

class ClipboardMimeDataRestorer final
{
public:
    ClipboardMimeDataRestorer()
        : m_mimeData(std::make_unique<QMimeData>())
    {
        const QMimeData* const originalMimeData =
            QApplication::clipboard()->mimeData(QClipboard::Clipboard);
        if (!originalMimeData)
        {
            return;
        }

        for (const QString& format : originalMimeData->formats())
        {
            m_mimeData->setData(format, originalMimeData->data(format));
        }
    }

    ~ClipboardMimeDataRestorer()
    {
        QApplication::clipboard()->setMimeData(
            m_mimeData.release(),
            QClipboard::Clipboard
            );
    }

    ClipboardMimeDataRestorer(const ClipboardMimeDataRestorer&) = delete;
    ClipboardMimeDataRestorer& operator=(
        const ClipboardMimeDataRestorer&
        ) = delete;

private:
    std::unique_ptr<QMimeData> m_mimeData;
};
}

class MainWindowEditActionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void undoAndRedoActionsRestoreAndReapplyPersonalNameInFocusedLineEdit();
    void pasteActionReplacesSelectedPersonalNameFromClipboard();
    void cutActionCopiesSelectedPersonalNameFromFocusedEditor();

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

void MainWindowEditActionParityTests::
pasteActionReplacesSelectedPersonalNameFromClipboard()
{
    ClipboardMimeDataRestorer clipboardMimeDataRestorer;
    QClipboard* const clipboard = QApplication::clipboard();
    QVERIFY(clipboard);
    clipboard->clear(QClipboard::Clipboard);

    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("paste-workspace.tps"))
        ).absoluteFilePath();
    const QString baselineName =
        QStringLiteral("F452 persisted personal name");

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

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);
    QCOMPARE(nameEditor->text(), baselineName);

    nameEditor->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    QVERIFY(nameEditor->hasFocus());
    QCOMPARE(QApplication::focusWidget(), nameEditor);

    QAction* const pasteAction = window.actions().paste;
    QVERIFY(pasteAction);
    QVERIFY(clipboard->text(QClipboard::Clipboard).isEmpty());
    QVERIFY(!pasteAction->isEnabled());

    const QString clipboardText =
        QStringLiteral("F452 pasted personal name");
    QVERIFY(!clipboardText.isEmpty());
    clipboard->setText(clipboardText, QClipboard::Clipboard);
    QTRY_VERIFY(pasteAction->isEnabled());

    QTest::keyClick(nameEditor, Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(nameEditor->selectedText(), baselineName);
    pasteAction->trigger();

    QCOMPARE(nameEditor->text(), clipboardText);
    QVERIFY(nameEditor->hasFocus());
    QCOMPARE(QApplication::focusWidget(), nameEditor);
}

void MainWindowEditActionParityTests::
cutActionCopiesSelectedPersonalNameFromFocusedEditor()
{
    ClipboardMimeDataRestorer clipboardMimeDataRestorer;
    QClipboard* const clipboard = QApplication::clipboard();
    QVERIFY(clipboard);

    const QString sentinelClipboardText =
        QStringLiteral("F453 sentinel clipboard text");
    clipboard->setText(sentinelClipboardText, QClipboard::Clipboard);
    QCOMPARE(clipboard->text(QClipboard::Clipboard), sentinelClipboardText);

    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("cut-workspace.tps"))
        ).absoluteFilePath();
    const QString baselineName =
        QStringLiteral("F453 persisted personal name");

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
    QVERIFY(QApplication::activeModalWidget() == nullptr);

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

    QTest::keyClick(nameEditor, Qt::Key_A, Qt::ControlModifier);
    const QString selectedName = nameEditor->selectedText();
    QCOMPARE(selectedName, baselineName);

    QAction* const cutAction = window.actions().cut;
    QVERIFY(cutAction);
    QTRY_VERIFY(cutAction->isEnabled());
    cutAction->trigger();

    QVERIFY(nameEditor->text().isEmpty());
    QCOMPARE(clipboard->text(QClipboard::Clipboard), selectedName);
    QVERIFY(nameEditor->hasFocus());
    QCOMPARE(QApplication::focusWidget(), nameEditor);
    QVERIFY(window.isVisible());
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

QTEST_MAIN(MainWindowEditActionParityTests)

#include "mainwindow_edit_action_parity_tests.moc"

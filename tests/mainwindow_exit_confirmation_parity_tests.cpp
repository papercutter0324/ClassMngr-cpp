#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/data/personal_details_repository.h"
#include "features/my_info/ui/personal_details_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QFileInfo>
#include <QLineEdit>
#include <QList>
#include <QStringList>
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

class UserPromptServiceScope final
{
public:
    explicit UserPromptServiceScope(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~UserPromptServiceScope()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};
}

class MainWindowExitConfirmationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void closeCancelPreservesDraftBeforeDiscardAcceptsClose();
    void closeSavePersistsPersonalName();
    void exitActionCancelPreservesDraftBeforeDiscardAcceptsClose();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowExitConfirmationParityTests::initTestCase()
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

void MainWindowExitConfirmationParityTests::
closeCancelPreservesDraftBeforeDiscardAcceptsClose()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

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
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Details);
    pages->setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);

    const QString persistedName =
        QStringLiteral("Persisted workspace profile");
    nameEditor->setText(persistedName);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(details->saveChanges());
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());

    const QString draft = QStringLiteral("Unsaved exit-confirmation draft");
    QVERIFY(draft != persistedName);
    nameEditor->setText(draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
    const QString activePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!activePageIdentifier.isEmpty());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    QVERIFY(!window.close());
    QVERIFY(window.isVisible());
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(services == window.services());
    QVERIFY(services->hasOpenDatabase());
    QVERIFY(services->databaseSession() == activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QCOMPARE(nameEditor->text(), draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    QVERIFY(window.close());
    QVERIFY(!window.isVisible());
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 2);
    QCOMPARE(nameEditor->text(), persistedName);
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());
}

void MainWindowExitConfirmationParityTests::closeSavePersistsPersonalName()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("save-on-exit-workspace.tps"))
        ).absoluteFilePath();

    const QString baselineName =
        QStringLiteral("F446 persisted baseline");
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

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

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
    pages->setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);

    QCOMPARE(nameEditor->text(), baselineName);
    const QString draftName = QStringLiteral("F446 close-save draft");
    QVERIFY(draftName != baselineName);
    nameEditor->setText(draftName);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 0);

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Save
        );
    QVERIFY(window.close());
    QVERIFY(!window.isVisible());
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());
    QCOMPARE(nameEditor->text(), draftName);
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    services->closeDatabase();

    ApplicationServices verificationServices;
    QVERIFY(verificationServices.openDatabase(workspacePath));
    QCOMPARE(
        PersonalDetailsRepository(verificationServices.settingsService())
            .load()
            .name,
        draftName
        );
    verificationServices.closeDatabase();
}

void MainWindowExitConfirmationParityTests::
exitActionCancelPreservesDraftBeforeDiscardAcceptsClose()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("exit-action-workspace.tps"))
        ).absoluteFilePath();

    const QString baselineName =
        QStringLiteral("F449 persisted baseline");
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

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

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
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Details);
    pages->setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);
    QCOMPARE(nameEditor->text(), baselineName);

    const QString draftName =
        QStringLiteral("F449 unsaved exit-action draft");
    QVERIFY(draftName != baselineName);
    nameEditor->setText(draftName);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
    const QString activePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!activePageIdentifier.isEmpty());

    QAction* const exitAction = window.actions().exitApp;
    QVERIFY(exitAction);
    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    exitAction->trigger();

    QVERIFY(window.isVisible());
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(services == window.services());
    QVERIFY(services->hasOpenDatabase());
    QVERIFY(services->databaseSession() == activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QCOMPARE(nameEditor->text(), draftName);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    exitAction->trigger();

    QVERIFY(!window.isVisible());
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 2);
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());

    services->closeDatabase();
    ApplicationServices verificationServices;
    QVERIFY(verificationServices.openDatabase(workspacePath));
    QCOMPARE(
        PersonalDetailsRepository(verificationServices.settingsService())
            .load()
            .name,
        baselineName
        );
    verificationServices.closeDatabase();
}

QTEST_MAIN(MainWindowExitConfirmationParityTests)

#include "mainwindow_exit_confirmation_parity_tests.moc"

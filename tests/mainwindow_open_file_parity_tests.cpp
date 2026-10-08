#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/my_info/data/personal_details_repository.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QAction>
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

class FileDialogServiceScope final
{
public:
    explicit FileDialogServiceScope(IFileDialogService* service)
    {
        DialogServices::setFileDialogServiceForTesting(service);
    }

    ~FileDialogServiceScope()
    {
        DialogServices::setFileDialogServiceForTesting(nullptr);
    }
};
}

class MainWindowOpenFileParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void openFileCancelPreservesDraftAndChooserCancelKeepsWorkspace();
    void openFileSaveThenChooserCancelPreservesWorkspace();
    void openFileLoadsOtherProfileAndSelectsWorkspaceSchedule();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowOpenFileParityTests::initTestCase()
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

void MainWindowOpenFileParityTests::
openFileCancelPreservesDraftAndChooserCancelKeepsWorkspace()
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
    FakeFileDialogService fileDialogs;
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

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
    QCOMPARE(nameEditor->text(), persistedName);
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());

    const QString draft = QStringLiteral("Unsaved open-file draft");
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

    QAction* const openFileAction = window.actions().openFile;
    QVERIFY(openFileAction);
    QVERIFY(openFileAction->isEnabled());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    openFileAction->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QCOMPARE(fileDialogs.openFileRequests.size(), 0);
    QVERIFY(services == window.services());
    QVERIFY(services->hasOpenDatabase());
    QVERIFY(services->databaseSession() == activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QVERIFY(workspace->isVisible());
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
    openFileAction->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 2);
    QCOMPARE(fileDialogs.openFileRequests.size(), 1);
    const OpenFileRequest& openFileRequest =
        fileDialogs.openFileRequests.constFirst();
    QVERIFY(openFileRequest.parent == &window);
    QVERIFY(
        openFileRequest.purpose == FileDialogPurpose::TeacherProfile
        );
    QVERIFY(services == window.services());
    QVERIFY(services->hasOpenDatabase());
    QVERIFY(services->databaseSession() == activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QVERIFY(workspace->isVisible());
    QCOMPARE(nameEditor->text(), persistedName);
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
}

void MainWindowOpenFileParityTests::
openFileSaveThenChooserCancelPreservesWorkspace()
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
    FakeFileDialogService fileDialogs;
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

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
    QCOMPARE(nameEditor->text(), persistedName);
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());

    const QString draft = QStringLiteral("Saved open-file draft");
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

    QAction* const openFileAction = window.actions().openFile;
    QVERIFY(openFileAction);
    QVERIFY(openFileAction->isEnabled());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Save
        );
    fileDialogs.scriptedOpenFiles.enqueue(std::nullopt);
    openFileAction->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QCOMPARE(fileDialogs.openFileRequests.size(), 1);
    const OpenFileRequest& openFileRequest =
        fileDialogs.openFileRequests.constFirst();
    QVERIFY(openFileRequest.parent == &window);
    QVERIFY(
        openFileRequest.purpose == FileDialogPurpose::TeacherProfile
        );

    QVERIFY(services == window.services());
    QVERIFY(services->hasOpenDatabase());
    QVERIFY(services->databaseSession() == activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QVERIFY(workspace->isVisible());
    QCOMPARE(nameEditor->text(), draft);
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
    QCOMPARE(
        PersonalDetailsRepository(services->settingsService()).load().name,
        draft
        );
}

void MainWindowOpenFileParityTests::
openFileLoadsOtherProfileAndSelectsWorkspaceSchedule()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString profileAPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("profile-a.tps"))
        ).absoluteFilePath();
    const QString profileBPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("profile-b.tps"))
        ).absoluteFilePath();
    QVERIFY(profileAPath != profileBPath);

    ApplicationServices profileASeed;
    QVERIFY(profileASeed.openDatabase(profileAPath));
    PersonalDetails profileADetails;
    profileADetails.name = QStringLiteral("F444 Profile A");
    profileADetails.campus = QStringLiteral("Profile A Campus");
    QVERIFY(
        PersonalDetailsRepository(profileASeed.settingsService())
            .save(profileADetails)
        );
    profileASeed.closeDatabase();

    ApplicationServices profileBSeed;
    QVERIFY(profileBSeed.openDatabase(profileBPath));
    PersonalDetails profileBDetails;
    profileBDetails.name = QStringLiteral("F444 Profile B");
    profileBDetails.campus = QStringLiteral("Profile B Campus");
    QVERIFY(
        PersonalDetailsRepository(profileBSeed.settingsService())
            .save(profileBDetails)
        );
    profileBSeed.closeDatabase();
    QVERIFY(QFileInfo::exists(profileAPath));
    QVERIFY(QFileInfo::exists(profileBPath));

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = profileAPath;

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), profileAPath);
    QCOMPARE(
        PersonalDetailsRepository(services->settingsService()).load().name,
        profileADetails.name
        );

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QVERIFY(!workspace->hasUnsavedChanges());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList expectedSidebarKeys{
        QStringLiteral("my_workspace")
    };
    QCOMPARE(sidebar->selectedKeys(), expectedSidebarKeys);

    QAction* const openFileAction = window.actions().openFile;
    QVERIFY(openFileAction);
    QVERIFY(openFileAction->isEnabled());

    fileDialogs.scriptedOpenFiles.enqueue(profileBPath);
    openFileAction->trigger();
    QApplication::processEvents();

    QCOMPARE(fileDialogs.openFileRequests.size(), 1);
    const OpenFileRequest& openFileRequest =
        fileDialogs.openFileRequests.constFirst();
    QCOMPARE(openFileRequest.parent, static_cast<QWidget*>(&window));
    QCOMPARE(openFileRequest.title, QStringLiteral("Open Teacher Profile"));
    QVERIFY(openFileRequest.purpose == FileDialogPurpose::TeacherProfile);
    QCOMPARE(
        openFileRequest.initialDirectory,
        QFileInfo(profileAPath).absoluteDir().absolutePath()
        );
    const QStringList expectedNameFilters{
        QStringLiteral("ClassMngr Teacher Profile (*.tps)"),
        QStringLiteral("Legacy Teacher Profile (*.db)")
    };
    QCOMPARE(
        openFileRequest.nameFilters,
        expectedNameFilters
        );
    QVERIFY(fileDialogs.scriptedOpenFiles.isEmpty());
    QVERIFY(fileDialogs.openFilesRequests.isEmpty());
    QVERIFY(fileDialogs.saveFileRequests.isEmpty());
    QVERIFY(fileDialogs.saveFileWithOptionsRequests.isEmpty());
    QVERIFY(fileDialogs.directoryRequests.isEmpty());

    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), profileBPath);
    const PersonalDetails activeDetails =
        PersonalDetailsRepository(services->settingsService()).load();
    QCOMPARE(activeDetails.name, profileBDetails.name);
    QCOMPARE(activeDetails.campus, profileBDetails.campus);

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QVERIFY(workspace->schedulePage());
    QVERIFY(workspace->schedulePage()->isVisible());
    QCOMPARE(sidebar->selectedKeys(), expectedSidebarKeys);

    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
}

QTEST_MAIN(MainWindowOpenFileParityTests)

#include "mainwindow_open_file_parity_tests.moc"

#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/my_info/ui/my_workspace_page.h"
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

class MainWindowManageCampusesParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cancelPreservesDirtyWorkspaceWhenManageCampusesIsTriggered();
    void discardOpensCampusInformationOnReusedDashboard();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowManageCampusesParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
}

void MainWindowManageCampusesParityTests::init()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowManageCampusesParityTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowManageCampusesParityTests::
cancelPreservesDirtyWorkspaceWhenManageCampusesIsTriggered()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    MainWindow window(
        [](const QString&) {},
        true,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    QVERIFY(window.isAdmin());
    QVERIFY(window.actions().manageCampuses);
    QVERIFY(window.actions().manageCampuses->isEnabled());

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QVERIFY(!pages->isPageInstantiated(PageType::CampusDashboard));

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Details);
    pages->setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);

    const QString persistedName =
        QStringLiteral("Persisted profile before Manage Campuses");
    nameEditor->setText(persistedName);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(details->saveChanges());
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());

    const QString draft =
        QStringLiteral("Unsaved profile draft canceled before Manage Campuses");
    nameEditor->setText(draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList workspaceSidebarKeys{
        QStringLiteral("my_workspace")
    };
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    const QString activePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!activePageIdentifier.isEmpty());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    window.actions().manageCampuses->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QCOMPARE(nameEditor->text(), draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(!pages->isCurrentPage(PageType::CampusDashboard));
    QVERIFY(!pages->isPageInstantiated(PageType::CampusDashboard));
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    QVERIFY(window.actions().saveFile->isEnabled());
    QVERIFY(window.actions().saveAsFile->isEnabled());
    QVERIFY(window.actions().exportAsFile->isEnabled());
    QVERIFY(window.actions().closeFile->isEnabled());
}

void MainWindowManageCampusesParityTests::
discardOpensCampusInformationOnReusedDashboard()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    const UserPromptServiceScope promptScope(&prompts);

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    MainWindow window(
        [](const QString&) {},
        true,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QApplication::processEvents();

    QVERIFY(window.isAdmin());
    QVERIFY(window.actions().manageCampuses);
    QVERIFY(window.actions().manageCampuses->isEnabled());

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));

    // Instantiate the real Dashboard, then leave its section on Maps before
    // returning to My Workspace. The next QAction invocation must reset the
    // reused page and Sidebar to Information after Discard is accepted.
    window.actions().manageCampuses->trigger();
    QApplication::processEvents();
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));

    CampusDashboardPage* const campus = pages->campusDashboard();
    QVERIFY(campus);
    campus->showMap();
    QApplication::processEvents();
    QCOMPARE(campus->currentSectionKey(), QStringLiteral("campus_map"));

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QCOMPARE(
        sidebar->selectedKeys(),
        (QStringList{
            QStringLiteral("campus_info"),
            QStringLiteral("campus_map")
        })
        );

    pages->showPage(PageType::MyWorkspace);
    sidebar->selectByKeys(
        QStringList{QStringLiteral("my_workspace")}
        );

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Details);
    pages->setSaveMode(SaveMode::Manual);

    PersonalDetailsPage* const details = workspace->personalDetailsPage();
    QVERIFY(details);
    QLineEdit* const nameEditor = personalNameEditor(details);
    QVERIFY(nameEditor);

    const QString persistedName =
        QStringLiteral("Persisted profile before Manage Campuses");
    nameEditor->setText(persistedName);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(details->saveChanges());
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());

    const QString draft =
        QStringLiteral("Unsaved profile draft discarded before Manage Campuses");
    nameEditor->setText(draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());

    const QStringList workspaceSidebarKeys{
        QStringLiteral("my_workspace")
    };
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    window.actions().manageCampuses->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QCOMPARE(pages->campusDashboard(), campus);
    QCOMPARE(campus->currentSectionKey(), QStringLiteral("campus_information"));
    QCOMPARE(
        sidebar->selectedKeys(),
        (QStringList{
            QStringLiteral("campus_info"),
            QStringLiteral("campus_information")
        })
        );
    QCOMPARE(nameEditor->text(), persistedName);
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);

    QVERIFY(window.actions().saveFile->isEnabled());
    QVERIFY(window.actions().saveAsFile->isEnabled());
    QVERIFY(window.actions().exportAsFile->isEnabled());
    QVERIFY(window.actions().closeFile->isEnabled());
}

QTEST_MAIN(MainWindowManageCampusesParityTests)

#include "mainwindow_manage_campuses_parity_tests.moc"

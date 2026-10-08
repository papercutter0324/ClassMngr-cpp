#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "features/my_info/ui/my_workspace_page.h"

#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

#include <utility>

namespace
{
constexpr int KeyRole = Qt::UserRole + 4;

QTreeWidgetItem* findItemByKey(
    QTreeWidgetItem* parent,
    const QString& key
    )
{
    if (!parent)
    {
        return nullptr;
    }

    if (parent->data(0, KeyRole).toString() == key)
    {
        return parent;
    }

    for (int index = 0; index < parent->childCount(); ++index)
    {
        if (QTreeWidgetItem* const found = findItemByKey(
                parent->child(index),
                key
                ))
        {
            return found;
        }
    }

    return nullptr;
}

QTreeWidgetItem* findItemByKey(
    QTreeWidget* tree,
    const QString& key
    )
{
    if (!tree)
    {
        return nullptr;
    }

    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        if (QTreeWidgetItem* const found = findItemByKey(
                tree->topLevelItem(index),
                key
                ))
        {
            return found;
        }
    }

    return nullptr;
}

QAction* findRecentAction(
    QMenu* menu,
    const QString& path
    )
{
    if (!menu)
    {
        return nullptr;
    }

    for (QAction* const action : menu->actions())
    {
        if (action->data().toString() == path)
        {
            return action;
        }
    }

    return nullptr;
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

class MainWindowRecentWorkspaceReopenParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void recentActionReturnsFromOtherPageToSameWorkspaceSchedule();
    void recentActionSwitchesActiveWorkspaceAndUpdatesHistoryWithoutChooser();
    void missingRecentActionPrunesAndPreservesActiveWorkspace();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowRecentWorkspaceReopenParityTests::initTestCase()
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

void MainWindowRecentWorkspaceReopenParityTests::
recentActionReturnsFromOtherPageToSameWorkspaceSchedule()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString activePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-workspace.tps"))
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(otherPath));
    seedServices.closeDatabase();
    QVERIFY(seedServices.openDatabase(activePath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({otherPath, activePath});
    settings.setLastFile(otherPath);
    settings.sync();

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

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(!services->hasOpenDatabase());
    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(!pages->isCurrentPage(PageType::MyWorkspace));

    QMenu* const recentMenu = window.actions().recentFilesMenu;
    QVERIFY(recentMenu);
    QVERIFY(recentMenu->actions().size() >= 2);
    QCOMPARE(recentMenu->actions().at(0)->data().toString(), otherPath);
    QCOMPARE(recentMenu->actions().at(1)->data().toString(), activePath);

    QAction* const initialActiveAction = findRecentAction(
        recentMenu,
        activePath
        );
    QVERIFY(initialActiveAction);
    initialActiveAction->trigger();
    QApplication::processEvents();

    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), activePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QVERIFY(pages->myWorkspacePage());
    QCOMPARE(
        pages->myWorkspacePage()->currentTab(),
        WorkspaceTab::Schedule
        );
    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{activePath, otherPath})
        );
    QCOMPARE(settings.getLastFile(), activePath);
    QVERIFY(window.actions().saveFile->isEnabled());
    QVERIFY(window.actions().saveAsFile->isEnabled());
    QVERIFY(window.actions().exportAsFile->isEnabled());
    QVERIFY(window.actions().closeFile->isEnabled());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    QTreeWidgetItem* const campusInformation = findItemByKey(
        tree,
        QStringLiteral("campus_information")
        );
    QVERIFY(campusInformation);
    for (
        QTreeWidgetItem* ancestor = campusInformation->parent();
        ancestor;
        ancestor = ancestor->parent()
        )
    {
        ancestor->setExpanded(true);
    }
    tree->scrollToItem(campusInformation);
    QApplication::processEvents();
    const QRect campusInformationRect = tree->visualItemRect(
        campusInformation
        );
    QVERIFY(campusInformationRect.isValid());
    QVERIFY(!campusInformationRect.isEmpty());

    QSignalSpy routeEvents(sidebar, &Sidebar::itemSelected);
    QVERIFY(routeEvents.isValid());
    QTest::mouseClick(
        tree->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        campusInformationRect.center()
        );
    QCOMPARE(routeEvents.count(), 1);
    QVERIFY(!pages->isCurrentPage(PageType::MyWorkspace));

    routeEvents.clear();
    QAction* const reopenedActiveAction = findRecentAction(
        recentMenu,
        activePath
        );
    QVERIFY(reopenedActiveAction);
    reopenedActiveAction->trigger();
    QApplication::processEvents();

    QCOMPARE(routeEvents.count(), 0);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), activePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QVERIFY(pages->myWorkspacePage());
    QCOMPARE(
        pages->myWorkspacePage()->currentTab(),
        WorkspaceTab::Schedule
        );
    QCOMPARE(sidebar->selectedKeys(), QStringList{QStringLiteral("my_workspace")});
    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{activePath, otherPath})
        );
    QCOMPARE(settings.getLastFile(), activePath);
    QVERIFY(window.actions().saveFile->isEnabled());
    QVERIFY(window.actions().saveAsFile->isEnabled());
    QVERIFY(window.actions().exportAsFile->isEnabled());
    QVERIFY(window.actions().closeFile->isEnabled());
}

void MainWindowRecentWorkspaceReopenParityTests::
recentActionSwitchesActiveWorkspaceAndUpdatesHistoryWithoutChooser()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString activePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-workspace.tps"))
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(activePath));
    seedServices.closeDatabase();
    QVERIFY(seedServices.openDatabase(otherPath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({otherPath, activePath});
    settings.setLastFile(activePath);
    settings.sync();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = activePath;

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
    QCOMPARE(services->currentDatabasePath(), activePath);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const activeWorkspace = pages->myWorkspacePage();
    QVERIFY(activeWorkspace);
    const QString activePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!activePageIdentifier.isEmpty());

    QMenu* const recentMenu = window.actions().recentFilesMenu;
    QVERIFY(recentMenu);
    QAction* const otherWorkspaceAction = findRecentAction(
        recentMenu,
        otherPath
        );
    QVERIFY(otherWorkspaceAction);

    otherWorkspaceAction->trigger();
    QApplication::processEvents();

    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(fileDialogs.openFileRequests.isEmpty());
    QVERIFY(fileDialogs.openFilesRequests.isEmpty());
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), otherPath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), activeWorkspace);
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(activeWorkspace->currentTab(), WorkspaceTab::Schedule);
    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{otherPath, activePath})
        );
    QCOMPARE(settings.getLastFile(), otherPath);

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
}

void MainWindowRecentWorkspaceReopenParityTests::
missingRecentActionPrunesAndPreservesActiveWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString activePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-workspace.tps"))
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-workspace.tps"))
        ).absoluteFilePath();
    const QString missingPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("missing-workspace.tps"))
        ).absoluteFilePath();
    QVERIFY(!QFileInfo::exists(missingPath));

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(activePath));
    seedServices.closeDatabase();
    QVERIFY(seedServices.openDatabase(otherPath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({missingPath, activePath, otherPath});
    settings.setLastFile(activePath);
    settings.sync();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = activePath;

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
    QCOMPARE(services->currentDatabasePath(), activePath);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const activeWorkspace = pages->myWorkspacePage();
    QVERIFY(activeWorkspace);
    QVERIFY(!activeWorkspace->hasUnsavedChanges());
    const QString activePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!activePageIdentifier.isEmpty());
    const WorkspaceTab activeTab = activeWorkspace->currentTab();

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );

    QMenu* const recentMenu = window.actions().recentFilesMenu;
    QVERIFY(recentMenu);
    QAction* const missingWorkspaceAction = findRecentAction(
        recentMenu,
        missingPath
        );
    QVERIFY(missingWorkspaceAction);
    QVERIFY(findRecentAction(recentMenu, otherPath));

    settings.setLastFile(missingPath);
    settings.sync();
    missingWorkspaceAction->trigger();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Missing File"));
    QVERIFY(warning.severity == PromptSeverity::Warning);
    QCOMPARE(fileDialogs.openFileRequests.size(), 0);
    QVERIFY(fileDialogs.openFilesRequests.isEmpty());

    QVERIFY(services->hasOpenDatabase());
    QVERIFY(services->databaseSession() == activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), activeWorkspace);
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(activeWorkspace->currentTab(), activeTab);
    QVERIFY(!activeWorkspace->hasUnsavedChanges());
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );

    QVERIFY(settings.getLastFile().isEmpty());
    const QStringList remainingRecentFiles = settings.getRecentFiles();
    QVERIFY(!remainingRecentFiles.contains(missingPath));
    QVERIFY(remainingRecentFiles.contains(activePath));
    QVERIFY(remainingRecentFiles.contains(otherPath));
    QVERIFY(!findRecentAction(recentMenu, missingPath));
    QVERIFY(findRecentAction(recentMenu, activePath));
    QVERIFY(findRecentAction(recentMenu, otherPath));
}

QTEST_MAIN(MainWindowRecentWorkspaceReopenParityTests)

#include "mainwindow_recent_workspace_reopen_parity_tests.moc"

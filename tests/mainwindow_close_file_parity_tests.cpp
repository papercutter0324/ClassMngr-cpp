#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
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
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

#include <utility>

namespace
{
constexpr int KeyRole = Qt::UserRole + 4;

const QStringList& databaseSidebarSectionKeys()
{
    static const QStringList keys{
        QStringLiteral("my_workspace"),
        QStringLiteral("sub_prep"),
        QStringLiteral("classes"),
        QStringLiteral("co_teachers"),
        QStringLiteral("campus_staff")
    };
    return keys;
}

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

class MainWindowCloseFileParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void closeFileCancelPreservesDraftBeforeDiscardClosesWorkspace();
    void closeFileSavePersistsDraftBeforeClosingWorkspace();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowCloseFileParityTests::initTestCase()
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

void MainWindowCloseFileParityTests::
closeFileCancelPreservesDraftBeforeDiscardClosesWorkspace()
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
    const QString draft = QStringLiteral("Unsaved workspace draft");
    nameEditor->setText(draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    for (const QString& key : databaseSidebarSectionKeys())
    {
        QTreeWidgetItem* const item = findItemByKey(tree, key);
        QVERIFY(item);
        QVERIFY(!item->isHidden());
    }

    const QList<QAction*> databaseBackedActions{
        window.actions().saveFile,
        window.actions().saveAsFile,
        window.actions().exportAsFile,
        window.actions().closeFile,
        window.actions().newClass,
        window.actions().deleteClass,
        window.actions().importClasses,
        window.actions().exportClasses,
        window.actions().newTeacher,
        window.actions().deleteTeacher,
        window.actions().upcomingBirthdays
    };
    // Entity-dependent actions may already be disabled in this empty workspace.
    QList<bool> databaseBackedActionEnabledStates;
    databaseBackedActionEnabledStates.reserve(databaseBackedActions.size());
    for (QAction* const action : databaseBackedActions)
    {
        QVERIFY(action);
        databaseBackedActionEnabledStates.append(action->isEnabled());
    }
    QVERIFY(window.actions().saveFile->isEnabled());
    QVERIFY(window.actions().saveAsFile->isEnabled());
    QVERIFY(window.actions().exportAsFile->isEnabled());
    QVERIFY(window.actions().closeFile->isEnabled());

    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );
    const QString activePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!activePageIdentifier.isEmpty());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    QVERIFY(window.actions().closeFile->isEnabled());
    window.actions().closeFile->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(services->hasOpenDatabase());
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
    for (const QString& key : databaseSidebarSectionKeys())
    {
        QTreeWidgetItem* const item = findItemByKey(tree, key);
        QVERIFY(item);
        QVERIFY(!item->isHidden());
    }
    for (int index = 0; index < databaseBackedActions.size(); ++index)
    {
        QCOMPARE(
            databaseBackedActions.at(index)->isEnabled(),
            databaseBackedActionEnabledStates.at(index)
            );
    }

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    window.actions().closeFile->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 2);
    QVERIFY(!services->hasOpenDatabase());
    QVERIFY(services->currentDatabasePath().isEmpty());
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QVERIFY(pages->campusDashboard());
    QCOMPARE(
        pages->campusDashboard()->currentSectionKey(),
        QStringLiteral("campus_information")
        );
    QCOMPARE(
        sidebar->selectedKeys(),
        (QStringList{
            QStringLiteral("campus_info"),
            QStringLiteral("campus_information")
        })
        );

    for (const QString& key : databaseSidebarSectionKeys())
    {
        QTreeWidgetItem* const item = findItemByKey(tree, key);
        QVERIFY(item);
        QVERIFY(item->isHidden());
    }
    for (QAction* const action : databaseBackedActions)
    {
        QVERIFY(!action->isEnabled());
    }
}

void MainWindowCloseFileParityTests::
closeFileSavePersistsDraftBeforeClosingWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("save-close-workspace.tps"))
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
    const QString draft = QStringLiteral("Saved workspace profile draft");
    nameEditor->setText(draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    QCOMPARE(
        sidebar->selectedKeys(),
        QStringList{QStringLiteral("my_workspace")}
        );

    QAction* const closeFileAction = window.actions().closeFile;
    QVERIFY(closeFileAction);
    QVERIFY(closeFileAction->isEnabled());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Save
        );
    closeFileAction->trigger();
    QApplication::processEvents();

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QCOMPARE(fileDialogs.openFileRequests.size(), 0);
    QCOMPARE(fileDialogs.openFilesRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileWithOptionsRequests.size(), 0);
    QCOMPARE(fileDialogs.directoryRequests.size(), 0);

    QVERIFY(!services->hasOpenDatabase());
    QVERIFY(services->currentDatabasePath().isEmpty());
    QVERIFY(pages->isCurrentPage(PageType::CampusDashboard));
    QVERIFY(pages->campusDashboard());
    QCOMPARE(
        pages->campusDashboard()->currentSectionKey(),
        QStringLiteral("campus_information")
        );
    QCOMPARE(
        sidebar->selectedKeys(),
        (QStringList{
            QStringLiteral("campus_info"),
            QStringLiteral("campus_information")
        })
        );

    const QList<QAction*> databaseBackedActions{
        window.actions().saveFile,
        window.actions().saveAsFile,
        window.actions().exportAsFile,
        window.actions().closeFile,
        window.actions().newClass,
        window.actions().deleteClass,
        window.actions().importClasses,
        window.actions().exportClasses,
        window.actions().newTeacher,
        window.actions().deleteTeacher,
        window.actions().upcomingBirthdays
    };
    for (QAction* const action : databaseBackedActions)
    {
        QVERIFY(action);
        QVERIFY(!action->isEnabled());
    }

    ApplicationServices reopenedServices;
    QVERIFY(reopenedServices.openDatabase(workspacePath));
    QVERIFY(reopenedServices.settingsService());
    QCOMPARE(
        reopenedServices.settingsService()->loadOrDefault(
            QStringLiteral("myInfo/name"),
            QString()
            ).toString(),
        draft
        );
    reopenedServices.closeDatabase();
}

QTEST_MAIN(MainWindowCloseFileParityTests)

#include "mainwindow_close_file_parity_tests.moc"

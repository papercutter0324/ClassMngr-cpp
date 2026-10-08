#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/teacher/ui/teacher_import_dialog.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <algorithm>
#include <functional>
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

int rowCount(QSqlDatabase database, const QString& table)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

QList<int> teacherImportTableCounts(ApplicationServices* services)
{
    if (!services || !services->databaseSession())
    {
        return {-1, -1, -1};
    }

    const QSqlDatabase database = services->databaseSession()->database();
    return {
        rowCount(database, QStringLiteral("teachers")),
        rowCount(database, QStringLiteral("native_english_teachers")),
        rowCount(database, QStringLiteral("gs_team"))
    };
}

struct TeacherImportDialogObservation final
{
    bool dialogObserved = false;
    bool dialogRejected = false;
    bool timedOut = false;
    bool fallbackRejectedModal = false;
};

void triggerWhileRejectingImportDialogIfShown(
    QWidget* timerContext,
    const std::function<void()>& trigger,
    TeacherImportDialogObservation& observation
    )
{
    QTimer poll;
    poll.setInterval(10);
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(&poll, &QTimer::timeout, timerContext, [&]
    {
        auto* dialog = qobject_cast<TeacherImportDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                dialog = qobject_cast<TeacherImportDialog*>(widget);
                if (dialog)
                {
                    break;
                }
            }
        }
        if (!dialog)
        {
            return;
        }

        observation.dialogObserved = true;
        poll.stop();
        dialog->reject();
        observation.dialogRejected = dialog->result() == QDialog::Rejected;
    });
    QObject::connect(&timeout, &QTimer::timeout, timerContext, [&]
    {
        observation.timedOut = true;
        poll.stop();

        QDialog* dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                auto* candidate = qobject_cast<QDialog*>(widget);
                if (candidate && candidate->isModal() && candidate->isVisible())
                {
                    dialog = candidate;
                    break;
                }
            }
        }
        if (dialog)
        {
            observation.fallbackRejectedModal = true;
            dialog->reject();
        }
    });

    poll.start();
    timeout.start(5000);
    trigger();
    poll.stop();
    timeout.stop();
}
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
    void importTeachersCancelPreservesDirtyWorkspace();
    void importTeachersDiscardRestoresDraftAndRejectsDialog();

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

void MainWindowManageCampusesParityTests::
importTeachersCancelPreservesDirtyWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("import-teachers-cancel.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

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
    QVERIFY(window.actions().importTeachers);
    QVERIFY(window.actions().importTeachers->isEnabled());

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
        QStringLiteral("Persisted teacher name before Import Teachers cancel");
    nameEditor->setText(persistedName);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(details->saveChanges());
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());

    const QString draft =
        QStringLiteral("Unsaved teacher name draft before Import Teachers cancel");
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
    QWidget* const activePageWidget = pages->currentWidget();
    QCOMPARE(activePageWidget, static_cast<QWidget*>(workspace));
    const QList<int> tableCountsBefore = teacherImportTableCounts(services);
    QVERIFY(std::all_of(tableCountsBefore.cbegin(), tableCountsBefore.cend(),
        [](int count) { return count >= 0; }));

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    TeacherImportDialogObservation dialogObservation;
    triggerWhileRejectingImportDialogIfShown(
        &window,
        [&] { window.actions().importTeachers->trigger(); },
        dialogObservation
        );

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(!dialogObservation.dialogObserved);
    QVERIFY(!dialogObservation.dialogRejected);
    QVERIFY(!dialogObservation.timedOut);
    QVERIFY(!dialogObservation.fallbackRejectedModal);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(prompts.scriptedChoices.isEmpty());
    QVERIFY(prompts.scriptedActionIds.isEmpty());
    QVERIFY(fileDialogs.openFileRequests.isEmpty());
    QVERIFY(fileDialogs.openFilesRequests.isEmpty());
    QVERIFY(fileDialogs.saveFileRequests.isEmpty());
    QVERIFY(fileDialogs.saveFileWithOptionsRequests.isEmpty());
    QVERIFY(fileDialogs.directoryRequests.isEmpty());

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(pages->currentWidget(), activePageWidget);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QCOMPARE(nameEditor->text(), draft);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(workspace->hasUnsavedChanges());
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QCOMPARE(teacherImportTableCounts(services), tableCountsBefore);
}

void MainWindowManageCampusesParityTests::
importTeachersDiscardRestoresDraftAndRejectsDialog()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("import-teachers-discard.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    const UserPromptServiceScope promptScope(&prompts);
    const FileDialogServiceScope fileDialogScope(&fileDialogs);

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
    QVERIFY(window.actions().importTeachers);
    QVERIFY(window.actions().importTeachers->isEnabled());

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
        QStringLiteral("Persisted teacher name before Import Teachers discard");
    nameEditor->setText(persistedName);
    QVERIFY(details->hasUnsavedChanges());
    QVERIFY(details->saveChanges());
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());

    const QString draft =
        QStringLiteral("Unsaved teacher name draft before Import Teachers discard");
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
    QWidget* const activePageWidget = pages->currentWidget();
    QCOMPARE(activePageWidget, static_cast<QWidget*>(workspace));
    const QList<int> tableCountsBefore = teacherImportTableCounts(services);
    QVERIFY(std::all_of(tableCountsBefore.cbegin(), tableCountsBefore.cend(),
        [](int count) { return count >= 0; }));

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    TeacherImportDialogObservation dialogObservation;
    triggerWhileRejectingImportDialogIfShown(
        &window,
        [&] { window.actions().importTeachers->trigger(); },
        dialogObservation
        );

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(dialogObservation.dialogObserved);
    QVERIFY(dialogObservation.dialogRejected);
    QVERIFY(!dialogObservation.timedOut);
    QVERIFY(!dialogObservation.fallbackRejectedModal);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(prompts.scriptedChoices.isEmpty());
    QVERIFY(prompts.scriptedActionIds.isEmpty());
    QVERIFY(fileDialogs.openFileRequests.isEmpty());
    QVERIFY(fileDialogs.openFilesRequests.isEmpty());
    QVERIFY(fileDialogs.saveFileRequests.isEmpty());
    QVERIFY(fileDialogs.saveFileWithOptionsRequests.isEmpty());
    QVERIFY(fileDialogs.directoryRequests.isEmpty());

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentPageIdentifier(), activePageIdentifier);
    QCOMPARE(pages->currentWidget(), activePageWidget);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QCOMPARE(nameEditor->text(), persistedName);
    QVERIFY(!details->hasUnsavedChanges());
    QVERIFY(!workspace->hasUnsavedChanges());
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QCOMPARE(teacherImportTableCounts(services), tableCountsBefore);
}

QTEST_MAIN(MainWindowManageCampusesParityTests)

#include "mainwindow_manage_campuses_parity_tests.moc"

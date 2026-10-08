#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_import_repository.h"
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
#include <QCheckBox>
#include <QDate>
#include <QDir>
#include <QDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QRadioButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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

struct TeacherImportSuccessDialogObservation final
{
    bool dialogObserved = false;
    bool dialogKeyMatched = false;
    bool browseButtonFound = false;
    bool browseClicked = false;
    bool pickerParentMatchedDialog = false;
    bool statusLabelFound = false;
    bool validFileStatusObserved = false;
    bool m1SelectFound = false;
    bool m2NoneFound = false;
    bool h1AllFound = false;
    bool firstM1CandidateFound = false;
    bool secondM1CandidateFound = false;
    bool requestedReviewApplied = false;
    bool importButtonFound = false;
    bool importButtonEnabled = false;
    bool planMatchesExpected = false;
    bool importClicked = false;
    bool dialogAccepted = false;
    bool timedOut = false;
    bool fallbackRejectedModal = false;
    QString selectedFilePath;
    QString failure;
    TeacherImportPlan plan;
};

bool teacherImportPlanMatchesExpectedReview(
    const TeacherImportPlan& plan
    )
{
    if (!plan.review
        || plan.sourceDate != QDate(2026, 9, 1)
        || plan.koreanTeachers.size() != 2
        || plan.nativeEnglishTeachers.size() != 1
        || plan.gsTeamMembers.size() != 1
        || plan.review->groupSelections.size() != 3
        || plan.review->groupSelections.at(0).level != QStringLiteral("M1")
        || plan.review->groupSelections.at(0).mode
            != TeacherImportSelectionMode::Selected
        || plan.review->groupSelections.at(0).selectedCandidateIndexes
            != QList<int>{0}
        || plan.review->groupSelections.at(1).level != QStringLiteral("M2")
        || plan.review->groupSelections.at(1).mode
            != TeacherImportSelectionMode::None
        || plan.review->groupSelections.at(2).level != QStringLiteral("H1")
        || plan.review->groupSelections.at(2).mode
            != TeacherImportSelectionMode::All)
    {
        return false;
    }

    return plan.koreanTeachers.at(0).teacherKr == QStringLiteral("홍길동")
        && plan.koreanTeachers.at(1).teacherKr == QStringLiteral("박민준")
        && plan.nativeEnglishTeachers.at(0).name == QStringLiteral("Alex")
        && plan.gsTeamMembers.at(0).name == QStringLiteral("Taylor");
}

void triggerImportTeachersWithSuccessDialogObserver(
    QWidget* timerContext,
    QAction* importTeachersAction,
    FakeFileDialogService& fileDialogs,
    const QString& fixturePath,
    TeacherImportSuccessDialogObservation& observation
    )
{
    QTimer poll;
    poll.setInterval(10);
    QTimer watchdog;
    watchdog.setSingleShot(true);

    QObject::connect(&poll, &QTimer::timeout, timerContext, [&]
    {
        TeacherImportDialog* dialog = qobject_cast<TeacherImportDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                dialog = qobject_cast<TeacherImportDialog*>(widget);
                if (dialog && dialog->isVisible())
                {
                    break;
                }
                dialog = nullptr;
            }
        }
        if (!dialog)
        {
            return;
        }

        observation.dialogObserved = true;
        observation.dialogKeyMatched =
            dialog->objectName() == QStringLiteral("teacherImportDialog")
            && dialog->dialogKey() == QStringLiteral("teacherImport");

        if (!observation.browseClicked)
        {
            auto* const browseButton = dialog->findChild<QPushButton*>(
                QStringLiteral("teacherImportBrowseButton")
                );
            observation.browseButtonFound = browseButton != nullptr;
            if (!browseButton)
            {
                observation.failure = QStringLiteral(
                    "The teacher import Browse button was not found.");
                poll.stop();
                dialog->reject();
                return;
            }

            observation.browseClicked = true;
            browseButton->click();
            observation.pickerParentMatchedDialog =
                !fileDialogs.openFileRequests.isEmpty()
                && fileDialogs.openFileRequests.constLast().parent == dialog;
            return;
        }

        auto* const statusLabel = dialog->findChild<QLabel*>(
            QStringLiteral("teacherImportValidationStatus")
            );
        observation.statusLabelFound = statusLabel != nullptr;
        if (!statusLabel
            || statusLabel->text() != QStringLiteral("Status: Valid File"))
        {
            return;
        }
        observation.validFileStatusObserved = true;

        auto* const selectM1 = dialog->findChild<QRadioButton*>(
            QStringLiteral("teacherImportSelect_M1")
            );
        auto* const noneM2 = dialog->findChild<QRadioButton*>(
            QStringLiteral("teacherImportNone_M2")
            );
        auto* const allH1 = dialog->findChild<QRadioButton*>(
            QStringLiteral("teacherImportAll_H1")
            );
        auto* const firstM1Candidate = dialog->findChild<QCheckBox*>(
            QStringLiteral("teacherImportCandidate_0_0")
            );
        auto* const secondM1Candidate = dialog->findChild<QCheckBox*>(
            QStringLiteral("teacherImportCandidate_0_1")
            );
        observation.m1SelectFound = selectM1 != nullptr;
        observation.m2NoneFound = noneM2 != nullptr;
        observation.h1AllFound = allH1 != nullptr;
        observation.firstM1CandidateFound = firstM1Candidate != nullptr;
        observation.secondM1CandidateFound = secondM1Candidate != nullptr;

        if (!selectM1 || !noneM2 || !allH1
            || !firstM1Candidate || !secondM1Candidate)
        {
            observation.failure = QStringLiteral(
                "The valid teacher import review is missing an expected "
                "group or M1 candidate control.");
            poll.stop();
            dialog->reject();
            return;
        }

        selectM1->setChecked(true);
        firstM1Candidate->setChecked(true);
        secondM1Candidate->setChecked(false);
        noneM2->setChecked(true);
        allH1->setChecked(true);
        observation.requestedReviewApplied =
            selectM1->isChecked()
            && firstM1Candidate->isChecked()
            && !secondM1Candidate->isChecked()
            && noneM2->isChecked()
            && allH1->isChecked();

        observation.plan = dialog->importPlan();
        observation.planMatchesExpected =
            teacherImportPlanMatchesExpectedReview(observation.plan);
        auto* const importButton = dialog->findChild<QPushButton*>(
            QStringLiteral("teacherImportAcceptButton")
            );
        observation.importButtonFound = importButton != nullptr;
        observation.importButtonEnabled =
            importButton && importButton->isEnabled();
        auto* const filePathEdit = dialog->findChild<QLineEdit*>(
            QStringLiteral("teacherImportFilePath")
            );
        observation.selectedFilePath =
            filePathEdit ? filePathEdit->text() : QString();

        if (!observation.requestedReviewApplied
            || !observation.planMatchesExpected
            || !observation.importButtonEnabled)
        {
            observation.failure = QStringLiteral(
                "The teacher import dialog did not produce the expected "
                "review plan with an enabled Import button.");
            poll.stop();
            dialog->reject();
            return;
        }

        QObject::connect(
            dialog,
            &QDialog::accepted,
            timerContext,
            [&observation]
            {
                observation.dialogAccepted = true;
            }
            );
        observation.importClicked = true;
        poll.stop();
        importButton->click();
    });
    QObject::connect(&watchdog, &QTimer::timeout, timerContext, [&]
    {
        observation.timedOut = true;
        poll.stop();

        QDialog* dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                auto* const candidate = qobject_cast<QDialog*>(widget);
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
    watchdog.start(15000);
    importTeachersAction->trigger();
    poll.stop();
    watchdog.stop();
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
    void importTeachersActionAppliesCheckedInWorkbookFromWorkspace();

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

void MainWindowManageCampusesParityTests::
importTeachersActionAppliesCheckedInWorkbookFromWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("import-teachers-success.tps"))
        ).absoluteFilePath();
    const QString fixturePath = QDir(
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
        ).filePath(QStringLiteral("fixtures/teacher_import/sectioned_review.xlsx"));
    QVERIFY(QFileInfo::exists(fixturePath));

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedOpenFiles.enqueue(fixturePath);
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
    QAction* const importTeachersAction = window.actions().importTeachers;
    QVERIFY(importTeachersAction);
    QVERIFY(importTeachersAction->isEnabled());

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    const QList<int> tableCountsBefore = teacherImportTableCounts(services);
    QCOMPARE(tableCountsBefore, (QList<int>{0, 0, 0}));
    QSqlQuery latestDateBefore(activeSession->database());
    QVERIFY(latestDateBefore.prepare(QStringLiteral(
        "SELECT value FROM app_settings WHERE key=?")));
    latestDateBefore.addBindValue(QString::fromLatin1(
        TeacherImportRepository::LatestSourceDateSetting));
    QVERIFY(latestDateBefore.exec());
    QVERIFY(!latestDateBefore.next());

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    QVERIFY(!workspace->hasUnsavedChanges());
    QWidget* const workspaceWidget = pages->currentWidget();
    QVERIFY(workspaceWidget);
    const QString workspacePageIdentifier = pages->currentPageIdentifier();
    QVERIFY(!workspacePageIdentifier.isEmpty());

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList workspaceSidebarKeys{
        QStringLiteral("my_workspace")
    };
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);

    TeacherImportSuccessDialogObservation dialogObservation;
    triggerImportTeachersWithSuccessDialogObserver(
        &window,
        importTeachersAction,
        fileDialogs,
        fixturePath,
        dialogObservation
        );

    QVERIFY(dialogObservation.dialogObserved);
    QVERIFY(dialogObservation.dialogKeyMatched);
    QVERIFY(dialogObservation.browseButtonFound);
    QVERIFY(dialogObservation.browseClicked);
    QVERIFY(dialogObservation.pickerParentMatchedDialog);
    QVERIFY(dialogObservation.statusLabelFound);
    QVERIFY(dialogObservation.validFileStatusObserved);
    QVERIFY(dialogObservation.m1SelectFound);
    QVERIFY(dialogObservation.m2NoneFound);
    QVERIFY(dialogObservation.h1AllFound);
    QVERIFY(dialogObservation.firstM1CandidateFound);
    QVERIFY(dialogObservation.secondM1CandidateFound);
    QVERIFY(dialogObservation.requestedReviewApplied);
    QVERIFY(dialogObservation.importButtonFound);
    QVERIFY(dialogObservation.importButtonEnabled);
    QVERIFY(dialogObservation.planMatchesExpected);
    QVERIFY(dialogObservation.importClicked);
    QVERIFY(dialogObservation.dialogAccepted);
    QVERIFY(!dialogObservation.timedOut);
    QVERIFY(!dialogObservation.fallbackRejectedModal);
    QVERIFY2(dialogObservation.failure.isEmpty(),
             qPrintable(dialogObservation.failure));
    QCOMPARE(dialogObservation.selectedFilePath, fixturePath);

    QVERIFY(fileDialogs.scriptedOpenFiles.isEmpty());
    QCOMPARE(fileDialogs.openFileRequests.size(), 1);
    QCOMPARE(fileDialogs.openFilesRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 0);
    QCOMPARE(fileDialogs.saveFileWithOptionsRequests.size(), 0);
    QCOMPARE(fileDialogs.directoryRequests.size(), 0);
    const OpenFileRequest openRequest = fileDialogs.openFileRequests.constFirst();
    QCOMPARE(openRequest.title, QStringLiteral("Select Teacher Import File"));
    QCOMPARE(openRequest.purpose, FileDialogPurpose::ImportWorkbook);
    QVERIFY(openRequest.initialDirectory.isEmpty());
    QCOMPARE(
        openRequest.nameFilters,
        QStringList{QStringLiteral("Excel Workbooks (*.xlsx)")}
        );

    QCOMPARE(teacherImportTableCounts(services), (QList<int>{2, 1, 1}));
    QSqlQuery koreanRecords(activeSession->database());
    QVERIFY(koreanRecords.exec(QStringLiteral(
        "SELECT id, teacher_kr, room_number, birthday, phone_number "
        "FROM teachers ORDER BY room_number")));
    QList<int> importedKoreanIds;
    QVERIFY(koreanRecords.next());
    importedKoreanIds.append(koreanRecords.value(0).toInt());
    QVERIFY(importedKoreanIds.constLast() > 0);
    QCOMPARE(koreanRecords.value(1).toString(), QStringLiteral("홍길동"));
    QCOMPARE(koreanRecords.value(2).toString(), QStringLiteral("413"));
    QCOMPARE(koreanRecords.value(3).toString(), QStringLiteral("02-29"));
    QCOMPARE(koreanRecords.value(4).toString(), QStringLiteral("010-1111-1111"));
    QVERIFY(koreanRecords.next());
    importedKoreanIds.append(koreanRecords.value(0).toInt());
    QVERIFY(importedKoreanIds.constLast() > 0);
    QCOMPARE(koreanRecords.value(1).toString(), QStringLiteral("박민준"));
    QCOMPARE(koreanRecords.value(2).toString(), QStringLiteral("510"));
    QCOMPARE(koreanRecords.value(3).toString(), QStringLiteral("05-09"));
    QCOMPARE(koreanRecords.value(4).toString(), QStringLiteral("010-4444-4444"));
    QVERIFY(!koreanRecords.next());

    QSqlQuery nativeEnglishRecord(activeSession->database());
    QVERIFY(nativeEnglishRecord.exec(QStringLiteral(
        "SELECT name FROM native_english_teachers")));
    QVERIFY(nativeEnglishRecord.next());
    QCOMPARE(nativeEnglishRecord.value(0).toString(), QStringLiteral("Alex"));
    QVERIFY(!nativeEnglishRecord.next());

    QSqlQuery gsTeamRecord(activeSession->database());
    QVERIFY(gsTeamRecord.exec(QStringLiteral("SELECT name FROM gs_team")));
    QVERIFY(gsTeamRecord.next());
    QCOMPARE(gsTeamRecord.value(0).toString(), QStringLiteral("Taylor"));
    QVERIFY(!gsTeamRecord.next());

    QSqlQuery latestDateAfter(activeSession->database());
    QVERIFY(latestDateAfter.prepare(QStringLiteral(
        "SELECT value FROM app_settings WHERE key=?")));
    latestDateAfter.addBindValue(QString::fromLatin1(
        TeacherImportRepository::LatestSourceDateSetting));
    QVERIFY(latestDateAfter.exec());
    QVERIFY(latestDateAfter.next());
    QCOMPARE(latestDateAfter.value(0).toString(), QStringLiteral("2026-09-01"));
    QVERIFY(!latestDateAfter.next());

    QTreeWidget* const sidebarTree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(sidebarTree);
    QTreeWidgetItem* campusStaff = nullptr;
    for (int index = 0; index < sidebarTree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* const item = sidebarTree->topLevelItem(index);
        if (item->data(0, Qt::UserRole + 4).toString()
            == QStringLiteral("campus_staff"))
        {
            campusStaff = item;
            break;
        }
    }
    QVERIFY(campusStaff);
    QTreeWidgetItem* koreanTeachers = nullptr;
    for (int index = 0; index < campusStaff->childCount(); ++index)
    {
        QTreeWidgetItem* const item = campusStaff->child(index);
        if (item->data(0, Qt::UserRole + 4).toString()
            == QStringLiteral("teachers_all_korean"))
        {
            koreanTeachers = item;
            break;
        }
    }
    QVERIFY(koreanTeachers);
    QCOMPARE(koreanTeachers->childCount(), 2);
    QList<int> sidebarTeacherIds;
    QStringList sidebarTeacherLabels;
    for (int index = 0; index < koreanTeachers->childCount(); ++index)
    {
        QTreeWidgetItem* const item = koreanTeachers->child(index);
        sidebarTeacherIds.append(item->data(0, Qt::UserRole + 3).toInt());
        sidebarTeacherLabels.append(item->text(0));
    }
    std::sort(importedKoreanIds.begin(), importedKoreanIds.end());
    std::sort(sidebarTeacherIds.begin(), sidebarTeacherIds.end());
    QCOMPARE(sidebarTeacherIds, importedKoreanIds);
    QVERIFY(std::any_of(sidebarTeacherLabels.cbegin(), sidebarTeacherLabels.cend(),
        [](const QString& label)
        {
            return label.contains(QStringLiteral("홍길동"));
        }));
    QVERIFY(std::any_of(sidebarTeacherLabels.cbegin(), sidebarTeacherLabels.cend(),
        [](const QString& label)
        {
            return label.contains(QStringLiteral("박민준"));
        }));

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& information = prompts.messages.constFirst();
    QCOMPARE(information.title, QStringLiteral("Import Teachers"));
    QCOMPARE(information.severity, PromptSeverity::Information);
    QCOMPARE(
        information.message,
        QStringLiteral(
            "Import complete.\n\n"
            "Korean Teachers: 2 created, 0 updated, 0 unchanged\n"
            "Native English Teachers: 1 created, 0 updated, 0 unchanged\n"
            "GS Team: 1 created, 0 updated, 0 unchanged")
        );
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(prompts.scriptedChoices.isEmpty());
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(prompts.scriptedActionIds.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->currentWidget(), workspaceWidget);
    QCOMPARE(pages->currentPageIdentifier(), workspacePageIdentifier);
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QVERIFY(activeSession->isOpen());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    QVERIFY(!workspace->hasUnsavedChanges());
}

QTEST_MAIN(MainWindowManageCampusesParityTests)

#include "mainwindow_manage_campuses_parity_tests.moc"

#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "domain/models/testing_class.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/schedule/ui/schedule_cell_hit_test.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QStringList>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

namespace
{
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

class MainWindowFixture final
{
public:
    MainWindowFixture()
        : promptScope(&prompts)
    {
    }

    bool initialize(QString* error = nullptr)
    {
        if (!workspaceRoot.isValid())
        {
            if (error)
            {
                *error = QStringLiteral("Workspace temporary directory is invalid.");
            }
            return false;
        }

        workspacePath = QFileInfo(
            workspaceRoot.filePath(QStringLiteral("handoff-workspace.tps"))
            ).absoluteFilePath();

        if (!seedServices.openDatabase(workspacePath))
        {
            if (error)
            {
                *error = QStringLiteral("Could not create the test workspace.");
            }
            return false;
        }
        seedServices.closeDatabase();

        if (!languageService.setLanguage(Language::English))
        {
            if (error)
            {
                *error = QStringLiteral("Could not select English for the test.");
            }
            return false;
        }

        MainWindowStartupOptions startupOptions;
        startupOptions.loadMostRecentDatabase = false;
        startupOptions.initialDatabasePath = workspacePath;
        window = std::make_unique<MainWindow>(
            [](const QString&) {},
            false,
            &languageService,
            nullptr,
            std::move(startupOptions)
            );
        window->show();
        QApplication::processEvents();

        if (
            !window->services()
            || !window->services()->hasOpenDatabase()
            )
        {
            if (error)
            {
                *error = QStringLiteral("MainWindow did not open the test workspace.");
            }
            return false;
        }
        return true;
    }

    QTemporaryDir workspaceRoot;
    QString workspacePath;
    ApplicationServices seedServices;
    LanguageService languageService;
    FakeUserPromptService prompts;
    UserPromptServiceScope promptScope;
    std::unique_ptr<MainWindow> window;
};

using TestingAssignmentDialogScript =
    std::function<bool(TestingAssignmentDialog*)>;

bool interactWithTestingAssignmentCell(
    ScheduleWidget& widget,
    const QString& day,
    const QString& timeLabel,
    const TestingAssignmentDialogScript& script
    )
{
    QTableWidget* const table = widget.findChild<QTableWidget*>(
        QStringLiteral("scheduleTable")
        );
    if (!table)
    {
        return false;
    }

    const ScheduleViewModel model = widget.scheduleModel();
    const int column = model.days.indexOf(day) + 1;
    int row = -1;
    for (qsizetype index = 0; index < model.rows.size(); ++index)
    {
        if (model.rows.at(index).timeLabel == timeLabel)
        {
            row = static_cast<int>(index);
            break;
        }
    }
    if (column <= 0 || row < 0)
    {
        return false;
    }

    QWidget* const cell = table->cellWidget(row, column);
    if (
        ScheduleCellHitTest::hit(cell).command
        != ScheduleCellCommand::EditTestingAssignment
        )
    {
        return false;
    }

    bool dialogFound = false;
    bool scriptSucceeded = false;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(
        &timer,
        &QTimer::timeout,
        &widget,
        [&]
        {
            for (QWidget* topLevel : QApplication::topLevelWidgets())
            {
                auto* dialog = qobject_cast<TestingAssignmentDialog*>(
                    topLevel
                    );
                if (!dialog)
                {
                    continue;
                }

                dialogFound = true;
                scriptSucceeded = script(dialog);
                if (!scriptSucceeded)
                {
                    dialog->reject();
                }
                return;
            }

            if (auto* modal = qobject_cast<TestingAssignmentDialog*>(
                    QApplication::activeModalWidget()
                    ))
            {
                modal->reject();
            }
        }
        );
    const QModelIndex cellIndex = table->model()->index(row, column);
    table->scrollTo(cellIndex);
    QApplication::processEvents();
    const QRect cellRect = table->visualRect(cellIndex);
    if (!cellRect.isValid())
    {
        timer.stop();
        return false;
    }
    timer.start(0);
    QTest::mouseClick(
        table->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        cellRect.center()
        );
    timer.stop();

    return dialogFound && scriptSucceeded;
}

bool showTestingMode(ScheduleWidget* widget)
{
    if (!widget)
    {
        return false;
    }

    auto* const modeButton = widget->findChild<QPushButton*>(
        QStringLiteral("scheduleTestingModeButton")
        );
    if (!modeButton || !modeButton->isEnabled())
    {
        return false;
    }
    modeButton->click();
    QApplication::processEvents();

    auto* const testingClassesButton = widget->findChild<QPushButton*>(
        QStringLiteral("scheduleTestingClassesButton")
        );
    return testingClassesButton && !testingClassesButton->isHidden();
}

QTreeWidgetItem* findChildItemByKey(
    QTreeWidgetItem* parent,
    const QString& key
    )
{
    if (!parent)
    {
        return nullptr;
    }

    for (int index = 0; index < parent->childCount(); ++index)
    {
        QTreeWidgetItem* const child = parent->child(index);
        if (child->data(0, Qt::UserRole + 4).toString() == key)
        {
            return child;
        }
    }

    return nullptr;
}

QTreeWidgetItem* findItemByKeyPath(
    QTreeWidget* tree,
    const QStringList& keys
    )
{
    if (!tree || keys.isEmpty())
    {
        return nullptr;
    }

    QTreeWidgetItem* item = nullptr;
    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* const candidate = tree->topLevelItem(index);
        if (candidate->data(0, Qt::UserRole + 4).toString() == keys.first())
        {
            item = candidate;
            break;
        }
    }

    for (qsizetype index = 1; item && index < keys.size(); ++index)
    {
        item = findChildItemByKey(item, keys.at(index));
    }

    return item;
}

QTreeWidgetItem* findTeacherLeaf(
    QTreeWidgetItem* group,
    const int teacherId
    )
{
    if (!group)
    {
        return nullptr;
    }

    for (int index = 0; index < group->childCount(); ++index)
    {
        QTreeWidgetItem* const child = group->child(index);
        if (child->data(0, Qt::UserRole + 4).toString()
                == QStringLiteral("teacher")
            && child->data(0, Qt::UserRole + 3).toInt() == teacherId)
        {
            return child;
        }
    }

    return nullptr;
}

bool scheduleModelContainsClassOnDay(
    const ScheduleViewModel& model,
    const int classId,
    const QString& day
    )
{
    for (const ScheduleRowView& row : model.rows)
    {
        for (const ScheduleCellView& cell : row.cells)
        {
            if (cell.day != day)
            {
                continue;
            }

            for (const ScheduleEntry& entry : cell.entries)
            {
                if (entry.classId == classId)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool clickTestingClassesButton(ScheduleWidget* widget)
{
    if (!widget)
    {
        return false;
    }

    auto* const button = widget->findChild<QPushButton*>(
        QStringLiteral("scheduleTestingClassesButton")
        );
    if (!button || !button->isEnabled() || button->isHidden())
    {
        return false;
    }
    button->click();
    QApplication::processEvents();
    return true;
}

void assertNoPromptRequests(const FakeUserPromptService& prompts)
{
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
}
}

class MainWindowScheduleTestingClassesHandoffParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void standaloneScheduleTestingClassesButtonReturnsToStandaloneSchedule();
    void workspaceScheduleTestingClassesButtonReturnsToWorkspaceSchedule();
    void dirtyTestingClassesReturnCancelThenDiscardUsesWorkspaceSource();
    void scheduleCellManageClassesPreservesRequestedSlotForNewClass();
    void scheduleImportPersistsAndRefreshesTeacherSidebar();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowScheduleTestingClassesHandoffParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
}

void MainWindowScheduleTestingClassesHandoffParityTests::init()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowScheduleTestingClassesHandoffParityTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowScheduleTestingClassesHandoffParityTests::
standaloneScheduleTestingClassesButtonReturnsToStandaloneSchedule()
{
    MainWindowFixture fixture;
    QString setupError;
    QVERIFY2(fixture.initialize(&setupError), qPrintable(setupError));

    MainWindow* const window = fixture.window.get();
    ApplicationServices* const services = window->services();
    PageManager* const pages = window->pageManager();
    QVERIFY(pages);
    QVERIFY(services);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);
    const QString activePath = services->currentDatabasePath();

    pages->showPage(PageType::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::Schedule));
    SchedulePage* const standaloneSchedule = pages->schedulePage();
    QVERIFY(standaloneSchedule);

    ScheduleWidget* const scheduleWidget =
        standaloneSchedule->findChild<ScheduleWidget*>();
    QVERIFY(scheduleWidget);
    QVERIFY(showTestingMode(scheduleWidget));

    Sidebar* const sidebar = window->findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList sourceSidebarKeys = sidebar->selectedKeys();

    QVERIFY(clickTestingClassesButton(scheduleWidget));
    QVERIFY(pages->isCurrentPage(PageType::TestingClasses));
    TestingClassesPage* const testingClasses =
        pages->testingClassesPage();
    QVERIFY(testingClasses);
    assertNoPromptRequests(fixture.prompts);

    auto* const backButton = testingClasses->findChild<QPushButton*>(
        QStringLiteral("testingClassesBackButton")
        );
    QVERIFY(backButton);
    backButton->click();
    QApplication::processEvents();

    assertNoPromptRequests(fixture.prompts);
    QVERIFY(pages->isCurrentPage(PageType::Schedule));
    QCOMPARE(pages->schedulePage(), standaloneSchedule);
    QCOMPARE(sidebar->selectedKeys(), sourceSidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
}

void MainWindowScheduleTestingClassesHandoffParityTests::
workspaceScheduleTestingClassesButtonReturnsToWorkspaceSchedule()
{
    MainWindowFixture fixture;
    QString setupError;
    QVERIFY2(fixture.initialize(&setupError), qPrintable(setupError));

    MainWindow* const window = fixture.window.get();
    ApplicationServices* const services = window->services();
    PageManager* const pages = window->pageManager();
    QVERIFY(pages);
    QVERIFY(services);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);
    const QString activePath = services->currentDatabasePath();

    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Schedule);
    QApplication::processEvents();

    SchedulePage* const workspaceSchedule = workspace->schedulePage();
    QVERIFY(workspaceSchedule);
    ScheduleWidget* const scheduleWidget =
        workspaceSchedule->findChild<ScheduleWidget*>();
    QVERIFY(scheduleWidget);
    QVERIFY(showTestingMode(scheduleWidget));

    Sidebar* const sidebar = window->findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList workspaceSidebarKeys{
        QStringLiteral("my_workspace")
    };
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);

    QVERIFY(clickTestingClassesButton(scheduleWidget));
    QVERIFY(pages->isCurrentPage(PageType::TestingClasses));
    TestingClassesPage* const testingClasses =
        pages->testingClassesPage();
    QVERIFY(testingClasses);
    auto* const backButton = testingClasses->findChild<QPushButton*>(
        QStringLiteral("testingClassesBackButton")
        );
    QVERIFY(backButton);
    backButton->click();
    QApplication::processEvents();

    assertNoPromptRequests(fixture.prompts);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
}

void MainWindowScheduleTestingClassesHandoffParityTests::
dirtyTestingClassesReturnCancelThenDiscardUsesWorkspaceSource()
{
    MainWindowFixture fixture;
    QString setupError;
    QVERIFY2(fixture.initialize(&setupError), qPrintable(setupError));

    MainWindow* const window = fixture.window.get();
    ApplicationServices* const services = window->services();
    PageManager* const pages = window->pageManager();
    QVERIFY(pages);
    QVERIFY(services);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);
    const QString activePath = services->currentDatabasePath();

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Schedule);
    QApplication::processEvents();

    SchedulePage* const workspaceSchedule = workspace->schedulePage();
    QVERIFY(workspaceSchedule);
    ScheduleWidget* const scheduleWidget =
        workspaceSchedule->findChild<ScheduleWidget*>();
    QVERIFY(scheduleWidget);
    QVERIFY(showTestingMode(scheduleWidget));
    QVERIFY(clickTestingClassesButton(scheduleWidget));
    QVERIFY(pages->isCurrentPage(PageType::TestingClasses));

    TestingClassesPage* const testingClasses =
        pages->testingClassesPage();
    QVERIFY(testingClasses);
    testingClasses->setSaveMode(SaveMode::Manual);
    QLineEdit* const nameEditor = testingClasses->findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    QVERIFY(nameEditor);

    const QString draft =
        QStringLiteral("Unsaved testing class handoff draft");
    nameEditor->setText(draft);
    QVERIFY(testingClasses->hasUnsavedChanges());

    Sidebar* const sidebar = window->findChild<Sidebar*>();
    QVERIFY(sidebar);
    const QStringList workspaceSidebarKeys{
        QStringLiteral("my_workspace")
    };
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);

    auto* const backButton = testingClasses->findChild<QPushButton*>(
        QStringLiteral("testingClassesBackButton")
        );
    QVERIFY(backButton);

    fixture.prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    backButton->click();
    QApplication::processEvents();

    QCOMPARE(fixture.prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(pages->isCurrentPage(PageType::TestingClasses));
    QCOMPARE(nameEditor->text(), draft);
    QVERIFY(testingClasses->hasUnsavedChanges());
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);

    fixture.prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    backButton->click();
    QApplication::processEvents();

    QCOMPARE(fixture.prompts.unsavedChangesConfirmations.size(), 2);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QVERIFY(!testingClasses->hasUnsavedChanges());
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
    QVERIFY(fixture.prompts.messages.isEmpty());
    QVERIFY(fixture.prompts.asynchronousMessages.isEmpty());
    QVERIFY(fixture.prompts.confirmations.isEmpty());
    QVERIFY(fixture.prompts.actionPrompts.isEmpty());
}

void MainWindowScheduleTestingClassesHandoffParityTests::
scheduleCellManageClassesPreservesRequestedSlotForNewClass()
{
    MainWindowFixture fixture;
    QString setupError;
    QVERIFY2(fixture.initialize(&setupError), qPrintable(setupError));

    MainWindow* const window = fixture.window.get();
    ApplicationServices* const services = window->services();
    PageManager* const pages = window->pageManager();
    QVERIFY(pages);
    QVERIFY(services);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);
    const QString activePath = services->currentDatabasePath();

    pages->showPage(PageType::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::Schedule));
    SchedulePage* const schedulePage = pages->schedulePage();
    QVERIFY(schedulePage);

    ScheduleWidget* const scheduleWidget =
        schedulePage->findChild<ScheduleWidget*>();
    QVERIFY(scheduleWidget);
    QVERIFY(showTestingMode(scheduleWidget));

    auto* scheduleService = services->scheduleService();
    QVERIFY(scheduleService);
    const auto initialTestingClasses = scheduleService->testingClasses();
    QVERIFY(initialTestingClasses);
    QVERIFY(initialTestingClasses->isEmpty());

    const QString day = QStringLiteral("Thursday");
    const QString startTime = QStringLiteral("17:00");
    ScheduleViewModel preview;
    preview.days = {day};
    ScheduleRowView row;
    row.timeLabel = startTime;
    row.timeRangeLabel = QStringLiteral("5:00 PM - 5:50 PM");
    ScheduleCellView cell;
    cell.day = day;
    cell.timeLabel = startTime;
    cell.defaultSlotState = scheduleTestingSlotState();
    cell.slotState = scheduleTestingSlotState();
    cell.testingBlockCreationEnabled = true;
    row.cells.append(cell);
    preview.rows.append(row);
    scheduleWidget->setPreviewModel(preview);

    QSignalSpy requested(
        schedulePage,
        &SchedulePage::testingClassesRequested
        );
    QVERIFY(requested.isValid());

    QVERIFY(interactWithTestingAssignmentCell(
        *scheduleWidget,
        day,
        startTime,
        [](TestingAssignmentDialog* dialog)
        {
            auto* const mode = dialog->findChild<QComboBox*>(
                QStringLiteral("testingAssignmentModeCombo")
                );
            auto* const classes = dialog->findChild<QComboBox*>(
                QStringLiteral("testingAssignmentClassCombo")
                );
            auto* const manage = dialog->findChild<QPushButton*>(
                QStringLiteral("testingAssignmentManageClassesButton")
                );
            if (!mode || !classes || !manage)
            {
                return false;
            }

            mode->setCurrentIndex(1);
            if (classes->count() != 0 || dialog->selectedClassId() > 0)
            {
                return false;
            }

            manage->click();
            return dialog->selectedAction()
                == TestingAssignmentDialog::Action::ManageTestingClasses;
        }
        ));

    QCOMPARE(requested.size(), 1);
    const QList<QVariant> arguments = requested.constFirst();
    QCOMPARE(arguments.size(), 3);
    QVERIFY(arguments.at(0).toInt() <= 0);
    QCOMPARE(arguments.at(1).toString(), day);
    QCOMPARE(arguments.at(2).toString(), startTime);
    QVERIFY(pages->isCurrentPage(PageType::TestingClasses));

    TestingClassesPage* const testingClasses =
        pages->testingClassesPage();
    QVERIFY(testingClasses);
    testingClasses->setSaveMode(SaveMode::Manual);
    QVERIFY(!testingClasses->hasUnsavedChanges());

    auto* const nameEditor = testingClasses->findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* const gradeCombo = testingClasses->findChild<QComboBox*>(
        QStringLiteral("testingClassGradeCombo")
        );
    auto* const levelCombo = testingClasses->findChild<QComboBox*>(
        QStringLiteral("testingClassLevelCombo")
        );
    auto* const roomEditor = testingClasses->findChild<QLineEdit*>(
        QStringLiteral("testingClassRoomEdit")
        );
    auto* const saveButton = testingClasses->findChild<QPushButton*>(
        QStringLiteral("testingClassesSaveButton")
        );
    QVERIFY(nameEditor);
    QVERIFY(gradeCombo);
    QVERIFY(levelCombo);
    QVERIFY(roomEditor);
    QVERIFY(saveButton);

    const QString createdName =
        QStringLiteral("Testing class from requested schedule slot");
    nameEditor->setText(createdName);
    gradeCombo->setCurrentText(QStringLiteral("M1"));
    levelCombo->setCurrentText(QStringLiteral("Major"));
    roomEditor->setText(QStringLiteral("Room 17"));
    QVERIFY(testingClasses->hasUnsavedChanges());
    saveButton->click();
    QApplication::processEvents();

    QVERIFY(!testingClasses->hasUnsavedChanges());
    const auto createdClasses = scheduleService->testingClasses();
    const auto savedAssignments = scheduleService->testingAssignments();
    QVERIFY(createdClasses);
    QVERIFY(savedAssignments);
    QCOMPARE(createdClasses->size(), 1);
    QCOMPARE(createdClasses->first().name, createdName);
    QVERIFY(createdClasses->first().classId > 0);
    QCOMPARE(savedAssignments->size(), 1);
    QCOMPARE(savedAssignments->first().classId, createdClasses->first().classId);
    QCOMPARE(savedAssignments->first().day, day);
    QCOMPARE(savedAssignments->first().startTime, startTime);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
    assertNoPromptRequests(fixture.prompts);
}

void MainWindowScheduleTestingClassesHandoffParityTests::
scheduleImportPersistsAndRefreshesTeacherSidebar()
{
    MainWindowFixture fixture;
    QString setupError;
    QVERIFY2(fixture.initialize(&setupError), qPrintable(setupError));

    MainWindow* const window = fixture.window.get();
    ApplicationServices* const services = window->services();
    PageManager* const pages = window->pageManager();
    QVERIFY(services);
    QVERIFY(pages);
    auto* const activeSession = services->databaseSession();
    QVERIFY(activeSession);
    const QString activePath = services->currentDatabasePath();

    const auto profileStatus = services->settingsService()->save(
        QStringLiteral("myInfo/name"),
        QStringLiteral("Alice")
        );
    QVERIFY2(
        profileStatus.has_value(),
        qPrintable(
            profileStatus.has_value() ? QString() : profileStatus.error()
            )
        );

    Sidebar* const sidebar = window->findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    const QStringList sidebarKeysBefore = sidebar->selectedKeys();
    const QStringList koreanTeacherGroupKeys{
        QStringLiteral("campus_staff"),
        QStringLiteral("teachers_all_korean")
    };
    QTreeWidgetItem* const koreanTeacherGroupBefore = findItemByKeyPath(
        tree,
        koreanTeacherGroupKeys
        );
    QVERIFY(koreanTeacherGroupBefore);

    pages->showPage(PageType::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::Schedule));
    SchedulePage* const schedulePage = pages->schedulePage();
    QVERIFY(schedulePage);
    ScheduleWidget* const scheduleWidget =
        schedulePage->findChild<ScheduleWidget*>();
    QVERIFY(scheduleWidget);
    QCOMPARE(scheduleWidget->runtimeMetrics().modelEntryCount, 0);

    auto* const importButton = scheduleWidget->findChild<QPushButton*>(
        QStringLiteral("scheduleImportButton")
        );
    QVERIFY(importButton);
    QVERIFY(importButton->isEnabled());
    QVERIFY(!importButton->isHidden());

    const QString workbookPath = QDir(
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        ).filePath(
            QStringLiteral("tests/fixtures/imports/schedule_review.xlsx")
            );
    QVERIFY2(
        QFileInfo::exists(workbookPath),
        qPrintable(QStringLiteral("Missing workbook: %1").arg(workbookPath))
        );

    QSignalSpy importRequested(
        schedulePage,
        &SchedulePage::scheduleImportRequested
        );
    QVERIFY(importRequested.isValid());

    int stage = 0;
    int attempts = 0;
    bool flowCompleted = false;
    QString flowFailure;
    QTimer dialogScript;
    dialogScript.setInterval(10);
    QObject::connect(
        &dialogScript,
        &QTimer::timeout,
        window,
        [&]()
        {
            ++attempts;
            ScheduleImportDialog* const dialog =
                window->findChild<ScheduleImportDialog*>();
            if (!dialog)
            {
                if (attempts > 2000)
                {
                    flowFailure = QStringLiteral(
                        "MainWindow did not open ScheduleImportDialog."
                        );
                    dialogScript.stop();
                }
                return;
            }

            const auto fail = [&](const QString& reason)
            {
                flowFailure = reason;
                dialogScript.stop();
                dialog->reject();
            };

            auto* const next = dialog->findChild<QPushButton*>(
                QStringLiteral("scheduleImportNextButton")
                );
            auto* const sheets = dialog->findChild<QComboBox*>(
                QStringLiteral("scheduleImportSheetCombo")
                );
            auto* const users = dialog->findChild<QComboBox*>(
                QStringLiteral("scheduleImportUserCombo")
                );
            auto* const progress = dialog->findChild<QProgressBar*>(
                QStringLiteral("scheduleImportProgressBar")
                );
            if (!next || !sheets || !users || !progress)
            {
                fail(QStringLiteral(
                    "ScheduleImportDialog is missing an expected control."
                    ));
                return;
            }

            if (stage == 0)
            {
                auto* const normal = dialog->findChild<QRadioButton*>(
                    QStringLiteral("scheduleImportNormalRadio")
                    );
                if (!normal)
                {
                    fail(QStringLiteral(
                        "ScheduleImportDialog is missing the Regular option."
                        ));
                    return;
                }

                dialog->setFilePath(workbookPath);
                normal->setChecked(true);
                next->click();
                stage = 1;
                return;
            }

            if (stage == 1)
            {
                if (!progress->isHidden())
                {
                    return;
                }
                if (sheets->count() == 0)
                {
                    auto* const sourceStatus = dialog->findChild<QLabel*>(
                        QStringLiteral("scheduleImportSourceStatus")
                        );
                    fail(
                        QStringLiteral("Workbook did not load: %1")
                            .arg(sourceStatus
                                ? sourceStatus->text()
                                : QStringLiteral("no worksheet loaded"))
                        );
                    return;
                }

                const int currentSheet = sheets->findText(
                    QStringLiteral("Current"),
                    Qt::MatchExactly
                    );
                if (currentSheet < 0)
                {
                    fail(QStringLiteral(
                        "The checked-in workbook has no Current sheet."
                        ));
                    return;
                }
                sheets->setCurrentIndex(currentSheet);

                const int alice = users->findText(
                    QStringLiteral("Alice"),
                    Qt::MatchExactly
                    );
                if (alice < 0)
                {
                    fail(QStringLiteral(
                        "The Current sheet has no Alice profile."
                        ));
                    return;
                }
                users->setCurrentIndex(alice);
                if (!next->isEnabled())
                {
                    fail(QStringLiteral(
                        "The Schedule Import review action is disabled."
                        ));
                    return;
                }
                next->click();
                stage = 2;
                return;
            }

            auto* const review =
                dialog->findChild<ScheduleImportReviewDialog*>();
            if (!review || !review->isVisible())
            {
                return;
            }

            for (int index = 0; index < 3; ++index)
            {
                auto* const teacherAction = review->findChild<QComboBox*>(
                    QStringLiteral("scheduleImportTeacherAction_%1")
                        .arg(index)
                    );
                auto* const classAction = review->findChild<QComboBox*>(
                    QStringLiteral("scheduleImportClassAction_%1")
                        .arg(index)
                    );
                if (!teacherAction || !classAction)
                {
                    fail(QStringLiteral(
                        "The workbook review is missing an import resolution."
                        ));
                    return;
                }

                int createTeacher = -1;
                for (int actionIndex = 0;
                     actionIndex < teacherAction->count();
                     ++actionIndex)
                {
                    if (teacherAction->itemData(actionIndex).toInt()
                        == static_cast<int>(ScheduleImportTeacherAction::Create))
                    {
                        createTeacher = actionIndex;
                        break;
                    }
                }
                int createClass = -1;
                for (int actionIndex = 0;
                     actionIndex < classAction->count();
                     ++actionIndex)
                {
                    if (classAction->itemData(actionIndex).toInt()
                        == static_cast<int>(ScheduleImportClassAction::CreateNew))
                    {
                        createClass = actionIndex;
                        break;
                    }
                }
                if (createTeacher < 0 || createClass < 0)
                {
                    fail(QStringLiteral(
                        "The workbook review cannot create the imported records."
                        ));
                    return;
                }
                teacherAction->setCurrentIndex(createTeacher);
                classAction->setCurrentIndex(createClass);
            }

            auto* const apply = review->findChild<QPushButton*>(
                QStringLiteral("scheduleImportAcceptButton")
                );
            if (!apply || !apply->isEnabled())
            {
                fail(QStringLiteral(
                    "The workbook review cannot apply the import."
                    ));
                return;
            }

            fixture.prompts.scriptedChoices.enqueue(
                PromptChoice::Accepted
                );
            apply->click();
            flowCompleted = true;
            dialogScript.stop();
        }
        );
    dialogScript.start();
    QTest::mouseClick(importButton, Qt::LeftButton);
    dialogScript.stop();
    if (flowFailure.isEmpty() && !flowCompleted)
    {
        flowFailure = QStringLiteral(
            "The Schedule Import dialog script did not complete."
            );
    }

    QCOMPARE(importRequested.size(), 1);
    QVERIFY2(flowCompleted, qPrintable(flowFailure));
    QVERIFY(pages->isCurrentPage(PageType::Schedule));
    QCOMPARE(pages->schedulePage(), schedulePage);
    QVERIFY(schedulePage->isVisible());
    QCOMPARE(sidebar->selectedKeys(), sidebarKeysBefore);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);

    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QCOMPARE(
        fixture.prompts.confirmations.constFirst().automationId,
        QStringLiteral("schedule-import-confirmation")
        );
    QCOMPARE(fixture.prompts.messages.size(), 1);
    QVERIFY(fixture.prompts.messages.constFirst().message.startsWith(
        QStringLiteral("Schedule imported successfully.")
        ));
    QVERIFY(fixture.prompts.asynchronousMessages.isEmpty());
    QVERIFY(fixture.prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(fixture.prompts.actionPrompts.isEmpty());

    const QString expectedTeacherName = QString::fromUtf8(
        "\xEB\xB0\x95\xEC\x84\xA0\xEC\x83\x9D"
        );
    const auto teachers = services->teacherService()->teachers();
    QVERIFY(teachers);
    int importedTeacherId = -1;
    for (const Teacher& teacher : *teachers)
    {
        if (teacher.teacherKr == expectedTeacherName)
        {
            importedTeacherId = teacher.id;
            QCOMPARE(teacher.roomNumber, QStringLiteral("415"));
            break;
        }
    }
    QVERIFY(importedTeacherId > 0);

    const auto classInfos = services->classService()->scheduleClassInfos();
    QVERIFY(classInfos);
    const ClassInfo* importedClassInfo = nullptr;
    for (const ClassInfo& info : *classInfos)
    {
        if (info.teacherId == importedTeacherId
            && info.classGrade == QStringLiteral("M3")
            && info.classLevel == QStringLiteral("Song's"))
        {
            importedClassInfo = &info;
            break;
        }
    }
    QVERIFY(importedClassInfo);
    QVERIFY(importedClassInfo->classId > 0);
    QCOMPARE(importedClassInfo->teacherKr, expectedTeacherName);
    QCOMPARE(importedClassInfo->roomNumber, QStringLiteral("415"));
    QCOMPARE(importedClassInfo->classTimes.size(), 2);
    QVERIFY(std::any_of(
        importedClassInfo->classTimes.cbegin(),
        importedClassInfo->classTimes.cend(),
        [](const ClassTime& time)
        {
            return time.day == QStringLiteral("Monday")
                && time.startTime == QStringLiteral("4:00 PM")
                && time.endTime == QStringLiteral("4:55 PM");
        }
        ));
    QVERIFY(std::any_of(
        importedClassInfo->classTimes.cbegin(),
        importedClassInfo->classTimes.cend(),
        [](const ClassTime& time)
        {
            return time.day == QStringLiteral("Friday")
                && time.startTime == QStringLiteral("4:00 PM")
                && time.endTime == QStringLiteral("4:55 PM");
        }
        ));

    QTreeWidgetItem* const koreanTeacherGroup = findItemByKeyPath(
        tree,
        koreanTeacherGroupKeys
        );
    QVERIFY(koreanTeacherGroup);
    QTreeWidgetItem* const importedTeacherLeaf = findTeacherLeaf(
        koreanTeacherGroup,
        importedTeacherId
        );
    QVERIFY(importedTeacherLeaf);

    const ScheduleViewModel visibleModel = scheduleWidget->scheduleModel();
    QVERIFY(scheduleModelContainsClassOnDay(
        visibleModel,
        importedClassInfo->classId,
        QStringLiteral("Monday")
        ));
    QVERIFY(scheduleModelContainsClassOnDay(
        visibleModel,
        importedClassInfo->classId,
        QStringLiteral("Friday")
        ));
}

QTEST_MAIN(MainWindowScheduleTestingClassesHandoffParityTests)

#include "mainwindow_schedule_testing_classes_handoff_parity_tests.moc"

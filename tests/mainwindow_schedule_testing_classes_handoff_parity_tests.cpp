#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "domain/models/testing_class.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
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
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStringList>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>
#include <QtTest>

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

QTEST_MAIN(MainWindowScheduleTestingClassesHandoffParityTests)

#include "mainwindow_schedule_testing_classes_handoff_parity_tests.moc"

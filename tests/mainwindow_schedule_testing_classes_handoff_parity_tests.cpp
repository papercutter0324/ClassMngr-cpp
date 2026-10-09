#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/testing_class.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/classes_page.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/schedule/ui/schedule_cell_hit_test.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSqlError>
#include <QSqlQuery>
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

bool clickSidebarItem(
    QTreeWidget* tree,
    QTreeWidgetItem* item
    )
{
    if (!tree || !item)
    {
        return false;
    }

    tree->scrollToItem(item);
    QApplication::processEvents();
    const QRect itemRect = tree->visualItemRect(item);
    if (!itemRect.isValid() || itemRect.isEmpty())
    {
        return false;
    }

    QTest::mouseClick(
        tree->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        itemRect.center()
        );
    QApplication::processEvents();
    return true;
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

bool scheduleModelContainsClassAndName(
    const ScheduleViewModel& model,
    const int classId,
    const QString& className
    )
{
    for (const ScheduleRowView& row : model.rows)
    {
        for (const ScheduleCellView& cell : row.cells)
        {
            for (const ScheduleEntry& entry : cell.entries)
            {
                if (entry.classId == classId
                    && entry.className == className)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool scheduleModelContainsEntryName(
    const ScheduleViewModel& model,
    const QString& className
    )
{
    for (const ScheduleRowView& row : model.rows)
    {
        for (const ScheduleCellView& cell : row.cells)
        {
            for (const ScheduleEntry& entry : cell.entries)
            {
                if (entry.className == className)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool seedClassWithSchedule(
    ApplicationServices& services,
    const QString& className,
    int* classId,
    QString* error
    )
{
    if (!classId)
    {
        if (error)
        {
            *error = QStringLiteral("Class ID output is required.");
        }
        return false;
    }

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD\uB2D8");
    teacher.teacherEn = QStringLiteral("Fixture Teacher");
    teacher.preferredRomanization = QStringLiteral("Fixture Teacher");
    teacher.preferredName = QStringLiteral("Fixture Teacher");
    teacher.roomNumber = QStringLiteral("F420 Room");
    teacher.wifiName = QStringLiteral("F420 WiFi");
    teacher.wifiPassword = QStringLiteral("f420-wifi-password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("123 456 7890");
    teacher.zoomPassword = QStringLiteral("f420-zoom-password");
    teacher.projectionType = QStringLiteral("Any");
    teacher.notes = QStringLiteral("F420 integration fixture teacher.");

    const auto teacherResult = services.teacherService()->create(teacher);
    if (!teacherResult || teacherResult.value() <= 0)
    {
        if (error)
        {
            *error = teacherResult
                ? QStringLiteral("Could not create an F420 teacher ID.")
                : QStringLiteral("Could not create the F420 teacher: %1")
                    .arg(teacherResult.error());
        }
        return false;
    }
    const int teacherId = teacherResult.value();

    const auto classResult = services.classService()->create(className);
    if (!classResult || classResult.value() <= 0)
    {
        if (error)
        {
            *error = classResult
                ? QStringLiteral("Could not create an F420 class ID.")
                : QStringLiteral("Could not create the F420 class: %1")
                    .arg(classResult.error());
        }
        return false;
    }
    const int createdClassId = classResult.value();

    const auto infoResult = services.classService()->classInfo(createdClassId);
    if (!infoResult)
    {
        if (error)
        {
            *error = QStringLiteral("Could not read the F420 class details: %1")
                .arg(infoResult.error());
        }
        return false;
    }

    ClassInfo info = infoResult.value();
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral("E5");
    info.classLevel = QStringLiteral("Artemis");
    info.classTimes = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:55 PM")
        }
    };
    const Status saveStatus = services.classService()->saveClassInfo(info);
    if (!saveStatus)
    {
        if (error)
        {
            *error = QStringLiteral("Could not save the F420 class setup: %1")
                .arg(saveStatus.error());
        }
        return false;
    }

    *classId = createdClassId;
    return true;
}

bool seedUnassignedClassWithSchedule(
    ApplicationServices& services,
    const QString& className,
    const QString& grade,
    const QString& level,
    const QString& day,
    const QString& startTime,
    const QString& endTime,
    int* classId,
    QString* error
    )
{
    if (!classId)
    {
        if (error)
        {
            *error = QStringLiteral("Class ID output is required.");
        }
        return false;
    }

    const auto created = services.classService()->create(className);
    if (!created || created.value() <= 0)
    {
        if (error)
        {
            *error = created
                ? QStringLiteral("Could not create an F443 class ID.")
                : QStringLiteral("Could not create the F443 class: %1")
                    .arg(created.error());
        }
        return false;
    }

    const int createdClassId = created.value();
    const auto loadedInfo = services.classService()->classInfo(createdClassId);
    if (!loadedInfo)
    {
        if (error)
        {
            *error = QStringLiteral("Could not read the F443 class details: %1")
                .arg(loadedInfo.error());
        }
        return false;
    }

    ClassInfo info = loadedInfo.value();
    info.teacherId = -1;
    info.classGrade = grade;
    info.classLevel = level;
    info.classTimes = {{day, startTime, endTime}};
    info.notes = className + QStringLiteral(" F443 fixture notes.");
    const Status saved = services.classService()->saveClassInfo(info);
    if (!saved)
    {
        if (error)
        {
            *error = QStringLiteral("Could not save the F443 class details: %1")
                .arg(saved.error());
        }
        return false;
    }

    *classId = createdClassId;
    return true;
}

int classScopedRowCount(
    const DatabaseSession& session,
    const QString& tableName,
    const int classId,
    const QString& keyColumnName = QStringLiteral("class_id")
    )
{
    QSqlQuery query(session.database());
    if (!query.prepare(
            QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2=?")
                .arg(tableName, keyColumnName)
            ))
    {
        return -1;
    }
    query.addBindValue(classId);
    return query.exec() && query.next()
        ? query.value(0).toInt()
        : -1;
}

bool sameClassTimes(
    const QList<ClassTime>& left,
    const QList<ClassTime>& right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (qsizetype index = 0; index < left.size(); ++index)
    {
        const ClassTime& leftTime = left.at(index);
        const ClassTime& rightTime = right.at(index);
        if (leftTime.day != rightTime.day
            || leftTime.startTime != rightTime.startTime
            || leftTime.endTime != rightTime.endTime)
        {
            return false;
        }
    }
    return true;
}

bool sameClassInfo(const ClassInfo& left, const ClassInfo& right)
{
    return left.classId == right.classId
        && left.teacherId == right.teacherId
        && left.teacherKr == right.teacherKr
        && left.teacherEn == right.teacherEn
        && left.teacherPreferredName == right.teacherPreferredName
        && left.roomNumber == right.roomNumber
        && left.wifiName == right.wifiName
        && left.wifiPassword == right.wifiPassword
        && left.internetType == right.internetType
        && left.zoomId == right.zoomId
        && left.zoomPassword == right.zoomPassword
        && left.projectionType == right.projectionType
        && left.classGrade == right.classGrade
        && left.classLevel == right.classLevel
        && left.readingBook == right.readingBook
        && left.essayBook == right.essayBook
        && left.classColor == right.classColor
        && left.fontColor == right.fontColor
        && sameClassTimes(left.classTimes, right.classTimes)
        && sameClassTimes(left.intensiveTimes, right.intensiveTimes)
        && left.notes == right.notes
        && left.timeFillerActivities == right.timeFillerActivities;
}

using ScheduleEditorDialogScript =
    std::function<bool(ScheduleEditorDialog*)>;

bool interactWithScheduleEntryCell(
    ScheduleWidget& widget,
    const int classId,
    const ScheduleEditorDialogScript& script
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
    for (qsizetype row = 0; row < model.rows.size(); ++row)
    {
        for (qsizetype dayIndex = 0;
             dayIndex < model.days.size();
             ++dayIndex)
        {
            const int column = static_cast<int>(dayIndex) + 1;
            QWidget* const cell = table->cellWidget(
                static_cast<int>(row),
                column
                );
            const ScheduleCellHit hit = ScheduleCellHitTest::hit(cell);
            if (hit.command != ScheduleCellCommand::EditClass
                || hit.classId != classId)
            {
                continue;
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
                        auto* dialog = qobject_cast<ScheduleEditorDialog*>(
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

                    if (auto* modal = qobject_cast<ScheduleEditorDialog*>(
                            QApplication::activeModalWidget()
                            ))
                    {
                        modal->reject();
                    }
                }
                );

            const QModelIndex cellIndex = table->model()->index(
                static_cast<int>(row),
                column
                );
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
    void classesDetailsSaveRefreshesClassActionsThroughMainWindow();
    void deleteClassActionSelectsTargetAndKeepsSiblingDetails();
    void workspaceScheduleEditorSaveRefreshesClassActionsThroughMainWindow();
    void testingClassRenameMarksAndRefreshesBothSchedulePages();
    void workspaceScheduleDisplayModeUpdatesLoadedClassesPage();

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
    QTreeWidgetItem* const classesItem = findItemByKeyPath(
        tree,
        QStringList{QStringLiteral("classes")}
        );
    QVERIFY(classesItem);
    QTreeWidgetItem* const koreanTeacherGroupBefore = findItemByKeyPath(
        tree,
        koreanTeacherGroupKeys
        );
    QVERIFY(koreanTeacherGroupBefore);

    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QCOMPARE(classesPage->runtimeMetrics().visibleClassCount, 0);

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

    bool cancellationDialogFound = false;
    QTimer::singleShot(
        0,
        window,
        [&]()
        {
            if (ScheduleImportDialog* const dialog =
                    window->findChild<ScheduleImportDialog*>())
            {
                cancellationDialogFound = true;
                dialog->reject();
            }
        }
        );
    QTest::mouseClick(importButton, Qt::LeftButton);
    QVERIFY(cancellationDialogFound);
    QCOMPARE(importRequested.size(), 1);
    QVERIFY(!classesPage->needsRefresh());

    const ClassesPageRuntimeMetrics classesBeforeImport =
        classesPage->runtimeMetrics();
    pages->showPage(PageType::Classes);
    const ClassesPageRuntimeMetrics afterCanceledImport =
        classesPage->runtimeMetrics();
    QCOMPARE(afterCanceledImport.classQueryCount,
        classesBeforeImport.classQueryCount);
    QCOMPARE(afterCanceledImport.classInfoQueryCount,
        classesBeforeImport.classInfoQueryCount);
    QVERIFY(!classesPage->needsRefresh());
    pages->showPage(PageType::Schedule);

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

    QCOMPARE(importRequested.size(), 2);
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

    QVERIFY(classesPage->needsRefresh());
    const ClassesPageRuntimeMetrics classesAfterAcceptedImport =
        classesPage->runtimeMetrics();
    QCOMPARE(classesAfterAcceptedImport.classQueryCount,
        classesBeforeImport.classQueryCount);
    QCOMPARE(classesAfterAcceptedImport.classInfoQueryCount,
        classesBeforeImport.classInfoQueryCount);

    QVERIFY(clickSidebarItem(tree, classesItem));
    QVERIFY(pages->isCurrentPage(PageType::Classes));

    const ClassesPageRuntimeMetrics classesAfterReentry =
        classesPage->runtimeMetrics();
    QCOMPARE(classesAfterReentry.classQueryCount,
        classesBeforeImport.classQueryCount + 1);
    QCOMPARE(classesAfterReentry.classInfoQueryCount,
        classesBeforeImport.classInfoQueryCount + 1);
    QVERIFY(!classesPage->needsRefresh());
    QVERIFY(classesAfterReentry.visibleClassCount >= 3);
    bool importedClassIsMaterialized = false;
    for (NavigationTabWidget* const tabs :
         classesPage->findChildren<NavigationTabWidget*>(
             QStringLiteral("classesLevelTabs")
             ))
    {
        for (int index = 0; index < tabs->count(); ++index)
        {
            const QWidget* const tabPage = tabs->widget(index);
            if (tabPage
                && tabPage->property("class_id").toInt()
                    == importedClassInfo->classId)
            {
                importedClassIsMaterialized = true;
            }
        }
    }
    QVERIFY(importedClassIsMaterialized);
}

void MainWindowScheduleTestingClassesHandoffParityTests::
classesDetailsSaveRefreshesClassActionsThroughMainWindow()
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

    QAction* const deleteClassAction = window->actions().deleteClass;
    QAction* const exportClassesAction = window->actions().exportClasses;
    QVERIFY(deleteClassAction);
    QVERIFY(exportClassesAction);
    QVERIFY(!deleteClassAction->isEnabled());
    QVERIFY(!exportClassesAction->isEnabled());

    int classId = -1;
    QString seedError;
    QVERIFY2(
        seedClassWithSchedule(
            *services,
            QStringLiteral("F420 Classes Details Save"),
            &classId,
            &seedError
            ),
        qPrintable(seedError)
        );
    QVERIFY(classId > 0);
    QVERIFY(!deleteClassAction->isEnabled());
    QVERIFY(!exportClassesAction->isEnabled());

    pages->showPage(PageType::Classes);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QVERIFY(classesPage->openClass(classId, ClassesSection::Details));
    QCOMPARE(classesPage->currentClassId(), classId);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);
    classesPage->setSaveMode(SaveMode::Manual);

    QComboBox* const grade = classesPage->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    QComboBox* const level = classesPage->findChild<QComboBox*>(
        QStringLiteral("classLevelCombo")
        );
    QComboBox* const readingBook = classesPage->findChild<QComboBox*>(
        QStringLiteral("classReadingBookCombo")
        );
    QComboBox* const essayBook = classesPage->findChild<QComboBox*>(
        QStringLiteral("classEssayBookCombo")
        );
    QPushButton* const saveButton = classesPage->findChild<QPushButton*>(
        QStringLiteral("classInfoSaveButton")
        );
    QVERIFY(grade);
    QVERIFY(level);
    QVERIFY(readingBook);
    QVERIFY(essayBook);
    QVERIFY(saveButton);

    QSignalSpy savedSpy(classesPage, &ClassesPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());

    grade->setCurrentText(QStringLiteral("E4"));
    level->setCurrentText(QStringLiteral("Theseus"));
    readingBook->setCurrentText(QStringLiteral("Reading Explorer 1"));
    essayBook->setCurrentText(QStringLiteral("4A"));
    QVERIFY(classesPage->hasUnsavedChanges());
    QVERIFY(!deleteClassAction->isEnabled());
    QVERIFY(!exportClassesAction->isEnabled());

    saveButton->click();
    QApplication::processEvents();

    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.constFirst().at(0).toInt(), classId);
    QVERIFY(deleteClassAction->isEnabled());
    QVERIFY(exportClassesAction->isEnabled());
    QVERIFY(!classesPage->hasUnsavedChanges());

    const auto persisted = services->classService()->classInfo(classId);
    QVERIFY(persisted);
    QCOMPARE(persisted->classId, classId);
    QCOMPARE(persisted->classGrade, QStringLiteral("E4"));
    QCOMPARE(persisted->classLevel, QStringLiteral("Theseus"));
    QCOMPARE(persisted->classTimes.size(), 1);
    QCOMPARE(
        persisted->classTimes.constFirst().day,
        QStringLiteral("Monday")
        );
    QCOMPARE(
        persisted->classTimes.constFirst().startTime,
        QStringLiteral("3:00 PM")
        );
    QCOMPARE(
        persisted->classTimes.constFirst().endTime,
        QStringLiteral("3:55 PM")
        );

    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
    assertNoPromptRequests(fixture.prompts);
}

void MainWindowScheduleTestingClassesHandoffParityTests::
deleteClassActionSelectsTargetAndKeepsSiblingDetails()
{
    MainWindowFixture fixture;
    QString setupError;
    QVERIFY2(fixture.initialize(&setupError), qPrintable(setupError));

    MainWindow* const window = fixture.window.get();
    ApplicationServices* const services = window->services();
    PageManager* const pages = window->pageManager();
    QVERIFY(services);
    QVERIFY(pages);
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);
    const QString activePath = services->currentDatabasePath();

    QAction* const deleteClassAction = window->actions().deleteClass;
    QVERIFY(deleteClassAction);
    QVERIFY(!deleteClassAction->isEnabled());

    int targetClassId = -1;
    int siblingClassId = -1;
    QString seedError;
    QVERIFY2(
        seedUnassignedClassWithSchedule(
            *services,
            QStringLiteral("F443 Target"),
            QStringLiteral("E5"),
            QStringLiteral("Artemis"),
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM"),
            &targetClassId,
            &seedError
            ),
        qPrintable(seedError)
        );
    QVERIFY2(
        seedUnassignedClassWithSchedule(
            *services,
            QStringLiteral("F443 Sibling"),
            QStringLiteral("E4"),
            QStringLiteral("Theseus"),
            QStringLiteral("Wednesday"),
            QStringLiteral("5:00 PM"),
            QStringLiteral("5:55 PM"),
            &siblingClassId,
            &seedError
            ),
        qPrintable(seedError)
        );
    QVERIFY(targetClassId > 0);
    QVERIFY(siblingClassId > 0);
    QVERIFY(targetClassId != siblingClassId);
    QVERIFY(!deleteClassAction->isEnabled());

    const auto originalTargetInfo =
        services->classService()->classInfo(targetClassId);
    const auto originalSiblingInfo =
        services->classService()->classInfo(siblingClassId);
    QVERIFY(originalTargetInfo);
    QVERIFY(originalSiblingInfo);
    QCOMPARE(originalTargetInfo->teacherId, -1);
    QCOMPARE(originalSiblingInfo->teacherId, -1);
    QCOMPARE(originalTargetInfo->classTimes.size(), 1);
    QCOMPARE(originalSiblingInfo->classTimes.size(), 1);
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_info"), targetClassId),
        1
        );
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_times"), targetClassId),
        1
        );

    pages->showPage(PageType::Classes);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QVERIFY(classesPage->openClass(targetClassId, ClassesSection::Details));
    QCOMPARE(classesPage->currentClassId(), targetClassId);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);
    classesPage->setSaveMode(SaveMode::Manual);

    QComboBox* const grade = classesPage->findChild<QComboBox*>(
        QStringLiteral("classGradeCombo")
        );
    QComboBox* const level = classesPage->findChild<QComboBox*>(
        QStringLiteral("classLevelCombo")
        );
    QComboBox* const readingBook = classesPage->findChild<QComboBox*>(
        QStringLiteral("classReadingBookCombo")
        );
    QComboBox* const essayBook = classesPage->findChild<QComboBox*>(
        QStringLiteral("classEssayBookCombo")
        );
    QPushButton* const saveButton = classesPage->findChild<QPushButton*>(
        QStringLiteral("classInfoSaveButton")
        );
    QVERIFY(grade);
    QVERIFY(level);
    QVERIFY(readingBook);
    QVERIFY(essayBook);
    QVERIFY(saveButton);

    QSignalSpy savedSpy(classesPage, &ClassesPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    grade->setCurrentText(QStringLiteral("E4"));
    level->setCurrentText(QStringLiteral("Theseus"));
    readingBook->setCurrentText(QStringLiteral("Reading Explorer 1"));
    essayBook->setCurrentText(QStringLiteral("4A"));
    QVERIFY(classesPage->hasUnsavedChanges());
    QVERIFY(!deleteClassAction->isEnabled());
    saveButton->click();
    QApplication::processEvents();
    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.constFirst().at(0).toInt(), targetClassId);
    QVERIFY(deleteClassAction->isEnabled());
    QVERIFY(!classesPage->hasUnsavedChanges());

    const auto targetInfo = services->classService()->classInfo(targetClassId);
    const auto siblingInfoBeforeDelete =
        services->classService()->classInfo(siblingClassId);
    QVERIFY(targetInfo);
    QVERIFY(siblingInfoBeforeDelete);
    QCOMPARE(targetInfo->teacherId, -1);
    QCOMPARE(targetInfo->classGrade, QStringLiteral("E4"));
    QCOMPARE(targetInfo->classLevel, QStringLiteral("Theseus"));
    QCOMPARE(targetInfo->classTimes.size(), 1);
    QCOMPARE(targetInfo->classTimes.constFirst().day, QStringLiteral("Monday"));
    QCOMPARE(targetInfo->classTimes.constFirst().startTime, QStringLiteral("4:00 PM"));
    QCOMPARE(targetInfo->classTimes.constFirst().endTime, QStringLiteral("4:55 PM"));
    QVERIFY(sameClassInfo(*originalSiblingInfo, *siblingInfoBeforeDelete));
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_info"), siblingClassId),
        1
        );
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_times"), targetClassId),
        1
        );
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_times"), siblingClassId),
        1
        );

    Sidebar* const sidebar = window->findChild<Sidebar*>();
    QVERIFY(sidebar);
    QCOMPARE(sidebar->getSelectedTeacherId(), -1);
    QWidget* const classesWidget = pages->currentWidget();
    QVERIFY(classesWidget);
    const QString expectedTargetLabel =
        QStringLiteral("E4 Theseus • No Teacher • Mon (4:00)");

    fixture.prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    bool chooserSeen = false;
    bool chooserAccepted = false;
    bool timedOut = false;
    bool fallbackRejectedModal = false;
    int selectedChooserClassId = -1;
    QString selectedChooserLabel;
    QString chooserFailure;

    QTimer chooserPoll;
    chooserPoll.setInterval(10);
    QObject::connect(
        &chooserPoll,
        &QTimer::timeout,
        window,
        [&]
        {
            QDialog* dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                for (QWidget* widget : QApplication::topLevelWidgets())
                {
                    auto* const candidate = qobject_cast<QDialog*>(widget);
                    if (candidate
                        && candidate->objectName()
                            == QStringLiteral("sidebarRecordSelectionDialog"))
                    {
                        dialog = candidate;
                        break;
                    }
                }
            }
            if (!dialog)
            {
                return;
            }
            if (dialog->objectName()
                != QStringLiteral("sidebarRecordSelectionDialog"))
            {
                chooserFailure = QStringLiteral(
                    "An unexpected modal dialog opened before class selection."
                    );
                fallbackRejectedModal = true;
                dialog->reject();
                return;
            }
            if (chooserSeen)
            {
                return;
            }

            chooserSeen = true;
            QComboBox* const combo = dialog->findChild<QComboBox*>(
                QStringLiteral("sidebarRecordSelectionCombo")
                );
            QDialogButtonBox* const buttons =
                dialog->findChild<QDialogButtonBox*>(
                    QStringLiteral("sidebarRecordSelectionButtonBox")
                    );
            QPushButton* const acceptButton = buttons
                ? buttons->button(QDialogButtonBox::Ok)
                : nullptr;
            if (!combo || !acceptButton)
            {
                chooserFailure = QStringLiteral(
                    "The class selection dialog controls were missing."
                    );
                dialog->reject();
                return;
            }

            const int targetIndex = combo->findData(targetClassId);
            if (targetIndex < 0)
            {
                chooserFailure = QStringLiteral(
                    "The target class was missing from the selection dialog."
                    );
                dialog->reject();
                return;
            }
            combo->setCurrentIndex(targetIndex);
            selectedChooserClassId = combo->currentData().toInt();
            selectedChooserLabel = combo->currentText();
            if (selectedChooserClassId != targetClassId
                || selectedChooserLabel != expectedTargetLabel
                || !acceptButton->isEnabled())
            {
                chooserFailure = QStringLiteral(
                    "The target class could not be selected for deletion."
                    );
                dialog->reject();
                return;
            }

            QObject::connect(
                dialog,
                &QDialog::accepted,
                window,
                [&chooserAccepted]
                {
                    chooserAccepted = true;
                }
                );
            acceptButton->click();
        }
        );

    QTimer chooserWatchdog;
    chooserWatchdog.setSingleShot(true);
    QObject::connect(
        &chooserWatchdog,
        &QTimer::timeout,
        window,
        [&]
        {
            if (chooserAccepted)
            {
                return;
            }
            timedOut = true;
            chooserFailure = QStringLiteral(
                "Timed out waiting for the class selection dialog."
                );

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
                dialog->reject();
            }
            else if (QWidget* const activeModal =
                         QApplication::activeModalWidget())
            {
                fallbackRejectedModal = true;
                activeModal->close();
            }
        }
        );

    chooserPoll.start();
    chooserWatchdog.start(5000);
    deleteClassAction->trigger();
    QApplication::processEvents();
    chooserPoll.stop();
    chooserWatchdog.stop();
    if (!chooserSeen && chooserFailure.isEmpty())
    {
        chooserFailure = QStringLiteral(
            "The class selection dialog did not open."
            );
    }

    QVERIFY2(chooserSeen, qPrintable(chooserFailure));
    QVERIFY2(chooserAccepted, qPrintable(chooserFailure));
    QVERIFY2(!timedOut, qPrintable(chooserFailure));
    QVERIFY2(!fallbackRejectedModal, qPrintable(chooserFailure));
    QVERIFY2(chooserFailure.isEmpty(), qPrintable(chooserFailure));
    QCOMPARE(selectedChooserClassId, targetClassId);
    QCOMPARE(selectedChooserLabel, expectedTargetLabel);

    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    const PromptRequest& confirmation = fixture.prompts.confirmations.constFirst();
    QCOMPARE(confirmation.parent, static_cast<QWidget*>(sidebar));
    QCOMPARE(confirmation.title, QStringLiteral("Delete Class"));
    QCOMPARE(
        confirmation.message,
        QStringLiteral("Delete 'E4 Theseus • No Teacher • Mon (4:00)'?")
        );
    QVERIFY(confirmation.severity == PromptSeverity::Warning);
    QCOMPARE(confirmation.acceptText, QStringLiteral("Delete"));
    QCOMPARE(confirmation.rejectText, QStringLiteral("Cancel"));
    QVERIFY(confirmation.destructive);
    QVERIFY(fixture.prompts.messages.isEmpty());
    QVERIFY(fixture.prompts.asynchronousMessages.isEmpty());
    QVERIFY(fixture.prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(fixture.prompts.actionPrompts.isEmpty());
    QVERIFY(fixture.prompts.scriptedChoices.isEmpty());

    const auto remainingClasses = services->classService()->classes();
    QVERIFY(remainingClasses);
    QCOMPARE(remainingClasses->size(), 1);
    QCOMPARE(remainingClasses->constFirst().id, siblingClassId);
    QCOMPARE(
        remainingClasses->constFirst().name,
        QStringLiteral("F443 Sibling")
        );
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_info"), targetClassId),
        0
        );
    const auto siblingInfoAfterDelete =
        services->classService()->classInfo(siblingClassId);
    QVERIFY(siblingInfoAfterDelete);
    QVERIFY(sameClassInfo(*originalSiblingInfo, *siblingInfoAfterDelete));
    QCOMPARE(
        classScopedRowCount(
            *activeSession,
            QStringLiteral("classes"),
            targetClassId,
            QStringLiteral("id")
            ),
        0
        );
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_info"), targetClassId),
        0
        );
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_times"), targetClassId),
        0
        );
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_info"), siblingClassId),
        1
        );
    QCOMPARE(
        classScopedRowCount(*activeSession, QStringLiteral("class_times"), siblingClassId),
        1
        );

    QVERIFY(pages->isCurrentPage(PageType::Classes));
    QCOMPARE(pages->classesPage(), classesPage);
    QCOMPARE(pages->currentWidget(), classesWidget);
    QCOMPARE(classesPage->currentClassId(), siblingClassId);
    QCOMPARE(classesPage->currentSection(), ClassesSection::Details);
    QVERIFY(!classesPage->hasUnsavedChanges());
    QCOMPARE(sidebar->selectedKeys(), QStringList{QStringLiteral("classes")});
    QCOMPARE(sidebar->getSelectedTeacherId(), -1);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
}

void MainWindowScheduleTestingClassesHandoffParityTests::
workspaceScheduleEditorSaveRefreshesClassActionsThroughMainWindow()
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

    QAction* const deleteClassAction = window->actions().deleteClass;
    QAction* const exportClassesAction = window->actions().exportClasses;
    QVERIFY(deleteClassAction);
    QVERIFY(exportClassesAction);
    QVERIFY(!deleteClassAction->isEnabled());
    QVERIFY(!exportClassesAction->isEnabled());

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    const QStringList workspaceSidebarKeys{
        QStringLiteral("my_workspace")
    };
    Sidebar* const sidebar = window->findChild<Sidebar*>();
    QVERIFY(sidebar);
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);

    SchedulePage* const schedulePage = workspace->schedulePage();
    QVERIFY(schedulePage);
    ScheduleWidget* const scheduleWidget =
        schedulePage->findChild<ScheduleWidget*>();
    QVERIFY(scheduleWidget);
    QCOMPARE(scheduleWidget->runtimeMetrics().modelEntryCount, 0);

    int classId = -1;
    QString seedError;
    QVERIFY2(
        seedClassWithSchedule(
            *services,
            QStringLiteral("F420 Workspace Schedule Save"),
            &classId,
            &seedError
            ),
        qPrintable(seedError)
        );
    QVERIFY(classId > 0);
    QVERIFY(!deleteClassAction->isEnabled());
    QVERIFY(!exportClassesAction->isEnabled());

    scheduleWidget->refreshSchedule();
    const ScheduleViewModel seededModel = scheduleWidget->scheduleModel();
    QCOMPARE(scheduleWidget->runtimeMetrics().modelEntryCount, 1);
    QVERIFY(scheduleModelContainsClassOnDay(
        seededModel,
        classId,
        QStringLiteral("Monday")
        ));

    pages->showPage(PageType::Classes);
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QCOMPARE(classesPage->currentClassId(), classId);
    const ClassesPageRuntimeMetrics classesBeforeScheduleSave =
        classesPage->runtimeMetrics();
    pages->showPage(PageType::MyWorkspace);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));

    QSignalSpy savedSpy(schedulePage, &SchedulePage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    QVERIFY(interactWithScheduleEntryCell(
        *scheduleWidget,
        classId,
        [](ScheduleEditorDialog* dialog)
        {
            if (!dialog)
            {
                return false;
            }

            const QList<QComboBox*> combos =
                dialog->findChildren<QComboBox*>();
            QPushButton* saveButton = nullptr;
            for (QPushButton* button : dialog->findChildren<QPushButton*>())
            {
                if (button->text() == QStringLiteral("Save"))
                {
                    saveButton = button;
                    break;
                }
            }
            if (combos.size() != 2 || !saveButton)
            {
                return false;
            }

            if (combos.at(0)->currentText() != QStringLiteral("E5")
                || combos.at(1)->currentText() != QStringLiteral("Artemis"))
            {
                return false;
            }

            combos.at(0)->setCurrentText(QStringLiteral("E4"));
            combos.at(1)->setCurrentText(QStringLiteral("Theseus"));
            QTest::mouseClick(saveButton, Qt::LeftButton);
            return !dialog->isVisible();
        }
        ));
    QApplication::processEvents();

    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.constFirst().at(0).toInt(), classId);
    QVERIFY(deleteClassAction->isEnabled());
    QVERIFY(exportClassesAction->isEnabled());

    const auto persisted = services->classService()->classInfo(classId);
    QVERIFY(persisted);
    QCOMPARE(persisted->classId, classId);
    QCOMPARE(persisted->classGrade, QStringLiteral("E4"));
    QCOMPARE(persisted->classLevel, QStringLiteral("Theseus"));
    QCOMPARE(persisted->classTimes.size(), 1);
    QCOMPARE(
        persisted->classTimes.constFirst().day,
        QStringLiteral("Monday")
        );
    QCOMPARE(
        persisted->classTimes.constFirst().startTime,
        QStringLiteral("3:00 PM")
        );
    QCOMPARE(
        persisted->classTimes.constFirst().endTime,
        QStringLiteral("3:55 PM")
        );

    QVERIFY(classesPage->needsRefresh());
    QCOMPARE(
        classesPage->runtimeMetrics().classQueryCount,
        classesBeforeScheduleSave.classQueryCount
        );
    QCOMPARE(
        classesPage->runtimeMetrics().classInfoQueryCount,
        classesBeforeScheduleSave.classInfoQueryCount
        );

    pages->showPage(PageType::Classes);
    const ClassesPageRuntimeMetrics classesAfterScheduleSave =
        classesPage->runtimeMetrics();
    QCOMPARE(
        classesAfterScheduleSave.classQueryCount,
        classesBeforeScheduleSave.classQueryCount + 1
        );
    QCOMPARE(
        classesAfterScheduleSave.classInfoQueryCount,
        classesBeforeScheduleSave.classInfoQueryCount + 1
        );
    QVERIFY(!classesPage->needsRefresh());
    QCOMPARE(classesPage->currentClassId(), classId);

    pages->showPage(PageType::MyWorkspace);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QVERIFY(workspace->schedulePage());
    scheduleWidget->refreshSchedule();

    QSqlQuery failClassInfoUpdate(activeSession->database());
    const QString createClassInfoFailureTrigger =
        QStringLiteral(
            "CREATE TRIGGER fail_f515_class_info_update "
            "BEFORE UPDATE ON class_info WHEN OLD.class_id = %1 "
            "BEGIN SELECT RAISE(ABORT, 'F515 test write failure'); END"
            ).arg(classId);
    QVERIFY2(
        failClassInfoUpdate.exec(createClassInfoFailureTrigger),
        qPrintable(failClassInfoUpdate.lastError().text())
        );

    QSignalSpy failedSaveSpy(schedulePage, &SchedulePage::classInfoSaved);
    QVERIFY(failedSaveSpy.isValid());
    bool failedSaveKeptEditorOpen = false;
    QVERIFY(!interactWithScheduleEntryCell(
        *scheduleWidget,
        classId,
        [&failedSaveKeptEditorOpen](ScheduleEditorDialog* dialog)
        {
            if (!dialog)
            {
                return false;
            }

            const QList<QComboBox*> combos =
                dialog->findChildren<QComboBox*>();
            QPushButton* saveButton = nullptr;
            for (QPushButton* button : dialog->findChildren<QPushButton*>())
            {
                if (button->text() == QStringLiteral("Save"))
                {
                    saveButton = button;
                    break;
                }
            }
            if (combos.size() != 2 || !saveButton)
            {
                return false;
            }

            if (combos.at(0)->currentText() != QStringLiteral("E4")
                || combos.at(1)->currentText() != QStringLiteral("Theseus"))
            {
                return false;
            }

            combos.at(0)->setCurrentText(QStringLiteral("E5"));
            combos.at(1)->setCurrentText(QStringLiteral("Artemis"));
            QTest::mouseClick(saveButton, Qt::LeftButton);
            failedSaveKeptEditorOpen = dialog->isVisible();
            return false;
        }
        ));
    QVERIFY(failedSaveKeptEditorOpen);
    QCOMPARE(failedSaveSpy.size(), 0);
    QVERIFY(!classesPage->needsRefresh());

    QSqlQuery dropClassInfoFailureTrigger(activeSession->database());
    QVERIFY(dropClassInfoFailureTrigger.exec(
        QStringLiteral("DROP TRIGGER fail_f515_class_info_update")
        ));

    const auto unchangedAfterFailedSave =
        services->classService()->classInfo(classId);
    QVERIFY(unchangedAfterFailedSave);
    QCOMPARE(unchangedAfterFailedSave->classGrade, QStringLiteral("E4"));
    QCOMPARE(unchangedAfterFailedSave->classLevel, QStringLiteral("Theseus"));

    pages->showPage(PageType::Classes);
    const ClassesPageRuntimeMetrics afterFailedSave =
        classesPage->runtimeMetrics();
    QCOMPARE(afterFailedSave.classQueryCount,
        classesAfterScheduleSave.classQueryCount);
    QCOMPARE(afterFailedSave.classInfoQueryCount,
        classesAfterScheduleSave.classInfoQueryCount);
    QVERIFY(!classesPage->needsRefresh());

    pages->showPage(PageType::MyWorkspace);
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QCOMPARE(workspace->schedulePage(), schedulePage);
    QCOMPARE(sidebar->selectedKeys(), workspaceSidebarKeys);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
    QVERIFY(!fixture.prompts.messages.isEmpty());
}

void MainWindowScheduleTestingClassesHandoffParityTests::
testingClassRenameMarksAndRefreshesBothSchedulePages()
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

    const QString originalName =
        QStringLiteral("F422 Original Testing Class");
    const QString updatedName =
        QStringLiteral("F422 Renamed Testing Class");
    TestingClass testingClass;
    testingClass.name = originalName;
    testingClass.grade = QStringLiteral("M1");
    testingClass.level = QStringLiteral("Major");
    testingClass.room = QStringLiteral("F422 Room");

    ScheduleService* const scheduleService = services->scheduleService();
    QVERIFY(scheduleService);
    const auto createdClass = scheduleService->createTestingClass(
        testingClass,
        QStringLiteral("Monday"),
        QStringLiteral("16:00")
        );
    if (!createdClass)
    {
        QFAIL(qPrintable(createdClass.error()));
    }
    const int classId = createdClass.value();
    QVERIFY(classId > 0);

    const auto assignments = scheduleService->testingAssignments();
    QVERIFY(assignments);
    QVERIFY(std::any_of(
        assignments->cbegin(),
        assignments->cend(),
        [classId](const TestingAssignment& assignment)
        {
            return assignment.classId == classId
                && assignment.day == QStringLiteral("Monday");
        }
        ));

    pages->showPage(PageType::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::Schedule));
    SchedulePage* const standaloneSchedule = pages->schedulePage();
    QVERIFY(standaloneSchedule);
    ScheduleWidget* const standaloneWidget =
        standaloneSchedule->findChild<ScheduleWidget*>();
    QVERIFY(standaloneWidget);
    QVERIFY(showTestingMode(standaloneWidget));
    QCOMPARE(
        standaloneWidget->displayState().displayMode,
        ScheduleDisplayMode::Testing
        );
    const ScheduleViewModel standaloneInitialModel =
        standaloneWidget->scheduleModel();
    QVERIFY(scheduleModelContainsClassOnDay(
        standaloneInitialModel,
        classId,
        QStringLiteral("Monday")
        ));
    QVERIFY(scheduleModelContainsClassAndName(
        standaloneInitialModel,
        classId,
        originalName
        ));
    QVERIFY(!standaloneSchedule->needsRefresh());

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    pages->showPage(PageType::MyWorkspace);
    workspace->openTab(WorkspaceTab::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    SchedulePage* const workspaceSchedule = workspace->schedulePage();
    QVERIFY(workspaceSchedule);
    QVERIFY(standaloneSchedule != workspaceSchedule);
    ScheduleWidget* const workspaceWidget =
        workspaceSchedule->findChild<ScheduleWidget*>();
    QVERIFY(workspaceWidget);
    QVERIFY(showTestingMode(workspaceWidget));
    QCOMPARE(
        workspaceWidget->displayState().displayMode,
        ScheduleDisplayMode::Testing
        );
    const ScheduleViewModel workspaceInitialModel =
        workspaceWidget->scheduleModel();
    QVERIFY(scheduleModelContainsClassOnDay(
        workspaceInitialModel,
        classId,
        QStringLiteral("Monday")
        ));
    QVERIFY(scheduleModelContainsClassAndName(
        workspaceInitialModel,
        classId,
        originalName
        ));
    QVERIFY(!workspaceSchedule->needsRefresh());

    QVERIFY(clickTestingClassesButton(workspaceWidget));
    QVERIFY(pages->isCurrentPage(PageType::TestingClasses));
    TestingClassesPage* const testingClasses =
        pages->testingClassesPage();
    QVERIFY(testingClasses);
    testingClasses->setSaveMode(SaveMode::Manual);
    QLineEdit* const nameEdit = testingClasses->findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    QPushButton* const saveButton = testingClasses->findChild<QPushButton*>(
        QStringLiteral("testingClassesSaveButton")
        );
    QVERIFY(nameEdit);
    QVERIFY(saveButton);
    QCOMPARE(nameEdit->text(), originalName);

    QSignalSpy testingDataChangedSpy(
        testingClasses,
        &TestingClassesPage::testingDataChanged
        );
    QVERIFY(testingDataChangedSpy.isValid());
    nameEdit->setText(updatedName);
    QVERIFY(testingClasses->hasUnsavedChanges());
    saveButton->click();

    QCOMPARE(testingDataChangedSpy.size(), 1);
    QVERIFY(standaloneSchedule->needsRefresh());
    QVERIFY(workspaceSchedule->needsRefresh());

    const auto persistedClass = scheduleService->testingClass(classId);
    QVERIFY(persistedClass);
    QCOMPARE(persistedClass->name, updatedName);
    QVERIFY(scheduleModelContainsClassAndName(
        standaloneWidget->scheduleModel(),
        classId,
        originalName
        ));
    QVERIFY(scheduleModelContainsClassAndName(
        workspaceWidget->scheduleModel(),
        classId,
        originalName
        ));

    pages->showPage(PageType::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::Schedule));
    QCOMPARE(pages->schedulePage(), standaloneSchedule);
    QVERIFY(!standaloneSchedule->needsRefresh());
    QCOMPARE(
        standaloneWidget->displayState().displayMode,
        ScheduleDisplayMode::Testing
        );
    const ScheduleViewModel standaloneUpdatedModel =
        standaloneWidget->scheduleModel();
    QVERIFY(scheduleModelContainsClassAndName(
        standaloneUpdatedModel,
        classId,
        updatedName
        ));
    QVERIFY(!scheduleModelContainsEntryName(
        standaloneUpdatedModel,
        originalName
        ));

    pages->showPage(PageType::MyWorkspace);
    workspace->openTab(WorkspaceTab::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QCOMPARE(workspace->schedulePage(), workspaceSchedule);
    QVERIFY(!workspaceSchedule->needsRefresh());
    QCOMPARE(
        workspaceWidget->displayState().displayMode,
        ScheduleDisplayMode::Testing
        );
    const ScheduleViewModel workspaceUpdatedModel =
        workspaceWidget->scheduleModel();
    QVERIFY(scheduleModelContainsClassAndName(
        workspaceUpdatedModel,
        classId,
        updatedName
        ));
    QVERIFY(!scheduleModelContainsEntryName(
        workspaceUpdatedModel,
        originalName
        ));

    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
    assertNoPromptRequests(fixture.prompts);
}

void MainWindowScheduleTestingClassesHandoffParityTests::
workspaceScheduleDisplayModeUpdatesLoadedClassesPage()
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
    ClassService* const classService = services->classService();
    QVERIFY(classService);

    int regularClassId = -1;
    QString regularSeedError;
    QVERIFY2(
        seedClassWithSchedule(
            *services,
            QStringLiteral("F423 Regular Tuesday"),
            &regularClassId,
            &regularSeedError
            ),
        qPrintable(regularSeedError)
        );

    const auto regularInfoResult = classService->classInfo(regularClassId);
    if (!regularInfoResult)
    {
        QFAIL(qPrintable(regularInfoResult.error()));
    }
    ClassInfo regularInfo = regularInfoResult.value();
    regularInfo.classTimes = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
        }
    };
    regularInfo.intensiveTimes.clear();
    const Status regularInfoSaved = classService->saveClassInfo(regularInfo);
    if (!regularInfoSaved)
    {
        QFAIL(qPrintable(regularInfoSaved.error()));
    }

    const auto intensiveClassCreated = classService->create(
        QStringLiteral("F423 Intensive Friday")
        );
    if (!intensiveClassCreated)
    {
        QFAIL(qPrintable(intensiveClassCreated.error()));
    }
    const int intensiveClassId = intensiveClassCreated.value();
    QVERIFY(intensiveClassId > 0);

    const auto intensiveInfoResult = classService->classInfo(intensiveClassId);
    if (!intensiveInfoResult)
    {
        QFAIL(qPrintable(intensiveInfoResult.error()));
    }
    ClassInfo intensiveInfo = intensiveInfoResult.value();
    intensiveInfo.teacherId = regularInfo.teacherId;
    intensiveInfo.classGrade = QStringLiteral("E5");
    intensiveInfo.classLevel = QStringLiteral("Artemis");
    intensiveInfo.classTimes.clear();
    intensiveInfo.intensiveTimes = {
        {
            QStringLiteral("Friday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
        }
    };
    const Status intensiveInfoSaved =
        classService->saveClassInfo(intensiveInfo);
    if (!intensiveInfoSaved)
    {
        QFAIL(qPrintable(intensiveInfoSaved.error()));
    }

    MyWorkspacePage* const workspace = pages->myWorkspacePage();
    QVERIFY(workspace);
    workspace->openTab(WorkspaceTab::Schedule);
    QApplication::processEvents();
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    SchedulePage* const mySchedulePage = workspace->schedulePage();
    QVERIFY(mySchedulePage);
    ScheduleWidget* const scheduleWidget =
        mySchedulePage->findChild<ScheduleWidget*>();
    QVERIFY(scheduleWidget);
    QCOMPARE(
        scheduleWidget->displayState().displayMode,
        ScheduleDisplayMode::Regular
        );
    QPushButton* const regularModeButton =
        scheduleWidget->findChild<QPushButton*>(
            QStringLiteral("scheduleRegularModeButton")
            );
    QPushButton* const intensiveModeButton =
        scheduleWidget->findChild<QPushButton*>(
            QStringLiteral("scheduleIntensiveModeButton")
            );
    QVERIFY(regularModeButton);
    QVERIFY(intensiveModeButton);
    QVERIFY(regularModeButton->isChecked());

    Sidebar* const sidebar = window->findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const sidebarTree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(sidebarTree);
    QTreeWidgetItem* const classesItem = findItemByKeyPath(
        sidebarTree,
        QStringList{QStringLiteral("classes")}
        );
    QTreeWidgetItem* const workspaceItem = findItemByKeyPath(
        sidebarTree,
        QStringList{QStringLiteral("my_workspace")}
        );
    QVERIFY(classesItem);
    QVERIFY(workspaceItem);

    QVERIFY(clickSidebarItem(sidebarTree, classesItem));
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    ClassesPage* const classesPage = pages->classesPage();
    QVERIFY(classesPage);
    QCOMPARE(classesPage->runtimeMetrics().visibleClassCount, 1);

    QPushButton* tuesdayFilterButton = classesPage->findChild<QPushButton*>(
        QStringLiteral("classesTuesdayFilterButton")
        );
    QVERIFY(tuesdayFilterButton);
    QVERIFY(!tuesdayFilterButton->isChecked());
    tuesdayFilterButton->click();
    QApplication::processEvents();
    QVERIFY(tuesdayFilterButton->isChecked());
    QCOMPARE(classesPage->runtimeMetrics().visibleClassCount, 1);
    const ClassesPageRuntimeMetrics classesBeforeLeave =
        classesPage->runtimeMetrics();

    QVERIFY(clickSidebarItem(sidebarTree, workspaceItem));
    QVERIFY(pages->isCurrentPage(PageType::MyWorkspace));
    QCOMPARE(pages->myWorkspacePage(), workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QVERIFY(scheduleWidget->isVisible());
    QCOMPARE(
        scheduleWidget->displayState().displayMode,
        ScheduleDisplayMode::Regular
        );

    intensiveModeButton->click();
    QApplication::processEvents();
    QCOMPARE(
        scheduleWidget->displayState().displayMode,
        ScheduleDisplayMode::Intensive
        );
    QVERIFY(intensiveModeButton->isChecked());

    QCOMPARE(pages->classesPage(), classesPage);
    QVERIFY(!classesPage->needsRefresh());
    QCOMPARE(classesPage->runtimeMetrics().navigationWidgetCount, 0);
    QVERIFY(classesPage->findChildren<NavigationTabWidget*>().isEmpty());
    QCOMPARE(
        classesPage->runtimeMetrics().visibleClassCount,
        classesBeforeLeave.visibleClassCount
        );
    QCOMPARE(
        classesPage->runtimeMetrics().classQueryCount,
        classesBeforeLeave.classQueryCount
        );
    QCOMPARE(
        classesPage->runtimeMetrics().classInfoQueryCount,
        classesBeforeLeave.classInfoQueryCount
        );

    QVERIFY(clickSidebarItem(sidebarTree, classesItem));
    QVERIFY(pages->isCurrentPage(PageType::Classes));
    QCOMPARE(pages->classesPage(), classesPage);
    const ClassesPageRuntimeMetrics classesAfterReentry =
        classesPage->runtimeMetrics();
    QCOMPARE(classesAfterReentry.visibleClassCount, 0);
    QCOMPARE(classesAfterReentry.classQueryCount,
        classesBeforeLeave.classQueryCount);
    QCOMPARE(classesAfterReentry.classInfoQueryCount,
        classesBeforeLeave.classInfoQueryCount);
    QVERIFY(classesAfterReentry.navigationWidgetCount > 0);
    tuesdayFilterButton = classesPage->findChild<QPushButton*>(
        QStringLiteral("classesTuesdayFilterButton")
        );
    QVERIFY(tuesdayFilterButton);
    QVERIFY(tuesdayFilterButton->isChecked());

    QVERIFY(clickSidebarItem(sidebarTree, classesItem));
    const ClassesPageRuntimeMetrics afterExplicitClassesSelection =
        classesPage->runtimeMetrics();
    QCOMPARE(afterExplicitClassesSelection.classQueryCount,
        classesAfterReentry.classQueryCount + 1);
    QCOMPARE(afterExplicitClassesSelection.classInfoQueryCount,
        classesAfterReentry.classInfoQueryCount + 1);

    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->databaseSession(), activeSession);
    QCOMPARE(services->currentDatabasePath(), activePath);
    assertNoPromptRequests(fixture.prompts);
}

QTEST_MAIN(MainWindowScheduleTestingClassesHandoffParityTests)

#include "mainwindow_schedule_testing_classes_handoff_parity_tests.moc"

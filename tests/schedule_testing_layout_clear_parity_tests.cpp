#include "app/mainwindow.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/testing_block_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "domain/models/testing_class.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_widget.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QQueue>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QVariant>
#include <QtTest/QtTest>

#include <memory>
#include <utility>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("testing-layout-clear-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

class RecordingPromptService final : public IUserPromptService
{
public:
    void showMessage(const PromptRequest& request) override
    {
        calls.append(request.severity == PromptSeverity::Warning
            ? QStringLiteral("warning")
            : QStringLiteral("message"));
        messages.append(request);
    }

    void showMessageAsync(const PromptRequest& request) override
    {
        calls.append(QStringLiteral("async-message"));
        asynchronousMessages.append(request);
    }

    PromptChoice confirm(const PromptRequest& request) override
    {
        calls.append(QStringLiteral("confirm"));
        confirmations.append(request);
        return choices.isEmpty()
            ? PromptChoice::Rejected
            : choices.dequeue();
    }

    UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest& request
        ) override
    {
        calls.append(QStringLiteral("leave-confirm"));
        unsavedChangesConfirmations.append(request);
        return UnsavedChangesChoice::Cancel;
    }

    QString chooseAction(const ActionPromptRequest& request) override
    {
        calls.append(QStringLiteral("choose-action"));
        actionPrompts.append(request);
        return {};
    }

    QStringList calls;
    QVector<PromptRequest> messages;
    QVector<PromptRequest> asynchronousMessages;
    QVector<PromptRequest> confirmations;
    QVector<UnsavedChangesRequest> unsavedChangesConfirmations;
    QVector<ActionPromptRequest> actionPrompts;
    QQueue<PromptChoice> choices;
};

struct SeedIds
{
    int firstTestingClass = -1;
    int secondTestingClass = -1;
    int unrelatedClass = -1;
    int regularM1Class = -1;
    int regularM2Class = -1;
};

bool executeSql(
    QSqlDatabase database,
    const QString& statement,
    const QList<QVariant>& bindings = {}
    )
{
    QSqlQuery query(database);
    if (!query.prepare(statement))
    {
        return false;
    }
    for (const QVariant& binding : bindings)
    {
        query.addBindValue(binding);
    }
    return query.exec();
}

int scalar(
    QSqlDatabase database,
    const QString& statement,
    const QVariant& binding = {}
    )
{
    QSqlQuery query(database);
    if (!query.prepare(statement))
    {
        return -1;
    }
    if (binding.isValid())
    {
        query.addBindValue(binding);
    }
    if (!query.exec() || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

QStringList layoutRows(QSqlDatabase database)
{
    QStringList rows;
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT day || '|' || start_time || '|' || room || '|' || "
            "COALESCE(class_id, -1) FROM schedule_testing_blocks "
            "ORDER BY day, start_time"
            )))
    {
        return {QStringLiteral("query-failed")};
    }
    while (query.next())
    {
        rows.append(query.value(0).toString());
    }
    return rows;
}

bool insertRoster(QSqlDatabase database, const int classId)
{
    return executeSql(
               database,
               QStringLiteral(
                   "INSERT INTO roster_columns "
                   "(class_id, name, position, width) "
                   "VALUES (?, 'Student', 0, 140)"
                   ),
               {classId}
               )
        && executeSql(
            database,
            QStringLiteral(
                "INSERT INTO roster_data "
                "(class_id, row_index, col_index, value) "
                "VALUES (?, 0, 0, 'Saved Student')"
                ),
            {classId}
            );
}

bool seedLayout(ApplicationServices& services, SeedIds* ids)
{
    if (!ids || !services.databaseSession())
    {
        return false;
    }
    DatabaseSession* const session = services.databaseSession();
    TestingClassRepository* const testingClasses =
        session->testingClassRepository();
    ClassRepository* const classes = session->classRepository();
    TestingBlockRepository* const blocks = session->testingBlockRepository();
    if (!testingClasses || !classes || !blocks)
    {
        return false;
    }

    TestingClass first;
    first.name = QStringLiteral("Saved Testing A");
    first.grade = QStringLiteral("M1");
    first.level = QStringLiteral("Solis");
    first.room = QStringLiteral("Room 101");
    const auto firstCreated = testingClasses->createTestingClass(first);
    if (!firstCreated)
    {
        return false;
    }
    ids->firstTestingClass = *firstCreated;

    TestingClass second;
    second.name = QStringLiteral("Saved Testing B");
    second.grade = QStringLiteral("M2");
    second.level = QStringLiteral("Leo");
    second.room = QStringLiteral("Room 202");
    const auto secondCreated = testingClasses->createTestingClass(second);
    if (!secondCreated)
    {
        return false;
    }
    ids->secondTestingClass = *secondCreated;

    const auto unrelatedCreated = classes->createClass(
        QStringLiteral("Unrelated Regular Class")
        );
    if (!unrelatedCreated)
    {
        return false;
    }
    ids->unrelatedClass = *unrelatedCreated;

    const QSqlDatabase database = session->database();
    if (!insertRoster(database, ids->firstTestingClass)
        || !insertRoster(database, ids->secondTestingClass)
        || !insertRoster(database, ids->unrelatedClass)
        || !executeSql(
            database,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, class_grade, class_level, notes) "
                "VALUES (?, 'E4', 'Orion', 'Keep unrelated class info')"
                ),
            {ids->unrelatedClass}
            )
        || !executeSql(
            database,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (?, 'Friday', '09:00', '10:00')"
                ),
            {ids->unrelatedClass}
            ))
    {
        return false;
    }

    return blocks->saveTestingBlock(
               QStringLiteral("Monday"),
               QStringLiteral("16:00"),
               QStringLiteral("Oral Room")
               )
        && blocks->assignTestingClass(
            QStringLiteral("Tuesday"),
            QStringLiteral("16:00"),
            ids->firstTestingClass
            )
        && blocks->assignTestingClass(
            QStringLiteral("Wednesday"),
            QStringLiteral("16:00"),
            ids->secondTestingClass
            );
}

bool seedRegularTestingPreferenceClassesInDatabase(
    ApplicationServices& services,
    SeedIds* ids
    )
{
    if (!ids || !services.databaseSession())
    {
        return false;
    }

    ClassRepository* const classes =
        services.databaseSession()->classRepository();
    if (!classes)
    {
        return false;
    }

    const auto createClass = [
        &services,
        classes
    ](
        const QString& name,
        const QString& grade,
        const QString& level,
        const QString& day,
        const QString& startTime,
        const QString& endTime,
        int* classId
    )
    {
        if (!classId)
        {
            return false;
        }

        const auto created = classes->createClass(name);
        if (!created)
        {
            return false;
        }
        *classId = *created;

        const QSqlDatabase database =
            services.databaseSession()->database();
        return executeSql(
                   database,
                   QStringLiteral(
                       "INSERT INTO class_info "
                       "(class_id, class_grade, class_level, notes) "
                       "VALUES (?, ?, ?, ?)"
                       ),
                   {*classId, grade, level, name}
                   )
            && executeSql(
                database,
                QStringLiteral(
                    "INSERT INTO class_times "
                    "(class_id, day, start_time, end_time) "
                    "VALUES (?, ?, ?, ?)"
                    ),
                {*classId, day, startTime, endTime}
                );
    };

    return createClass(
               QStringLiteral("Regular M1 Parity Class"),
               QStringLiteral("M1"),
               QStringLiteral("Solis"),
               QStringLiteral("Monday"),
               QStringLiteral("16:00"),
               QStringLiteral("16:50"),
               &ids->regularM1Class
               )
        && createClass(
            QStringLiteral("Regular M2 Parity Class"),
            QStringLiteral("M2"),
            QStringLiteral("Leo"),
            QStringLiteral("Tuesday"),
            QStringLiteral("17:00"),
            QStringLiteral("17:50"),
            &ids->regularM2Class
            );
}

class LayoutClearFixture final
{
public:
    explicit LayoutClearFixture(const bool withDatabase)
    {
        DialogServices::setUserPromptServiceForTesting(&prompts);
        MainWindowStartupOptions options;
        options.loadMostRecentDatabase = false;
        if (withDatabase)
        {
            options.initialDatabasePath = databasePath(directory);
            QFile emptyDatabase(options.initialDatabasePath);
            databaseFileReady = emptyDatabase.open(QIODevice::WriteOnly);
            if (databaseFileReady)
            {
                emptyDatabase.close();
            }
        }
        window = std::make_unique<MainWindow>(
            [](const QString&) {},
            false,
            nullptr,
            nullptr,
            std::move(options)
            );
        window->show();
        QApplication::processEvents();
        if (withDatabase)
        {
            databaseReady = window->services()
                && window->services()->databaseSession()
                && window->services()->databaseSession()->isOpen();
        }
    }

    ~LayoutClearFixture()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    bool prepareScheduleViews()
    {
        PageManager* const pages = window ? window->pageManager() : nullptr;
        if (!pages)
        {
            scheduleViewError = QStringLiteral("page manager is unavailable");
            return false;
        }

        pages->showPage(PageType::Schedule);
        standaloneSchedule = pages->schedulePage();
        if (!standaloneSchedule)
        {
            scheduleViewError = QStringLiteral("standalone Schedule page is unavailable");
            return false;
        }
        if (!setTestingMode(standaloneSchedule, &scheduleViewError))
        {
            return false;
        }

        workspace = pages->ensureMyWorkspacePage();
        if (!workspace)
        {
            scheduleViewError = QStringLiteral("My Workspace page is unavailable");
            return false;
        }
        workspace->openTab(WorkspaceTab::Schedule);
        pages->showPage(PageType::MyWorkspace);
        workspaceSchedule = workspace->schedulePage();
        if (!workspaceSchedule)
        {
            scheduleViewError = QStringLiteral("workspace Schedule page is unavailable");
            return false;
        }
        return setTestingMode(workspaceSchedule, &scheduleViewError);
    }

    bool invokeClearPreferences()
    {
        QAction* const action = window
            ? window->findChild<QAction*>(QStringLiteral("preferencesAction"))
            : nullptr;
        if (!action)
        {
            return false;
        }

        QTimer::singleShot(0, window.get(), [this]()
        {
            auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                return;
            }
            foundDialog = true;
            auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")
                );
            auto* const scheduleTab = dialog->findChild<QWidget*>(
                QStringLiteral("preferencesScheduleTab")
                );
            if (tabs && scheduleTab)
            {
                tabs->setCurrentWidget(scheduleTab);
                foundScheduleTab = true;
            }
            auto* const clearButton = dialog->findChild<QPushButton*>(
                QStringLiteral("preferencesScheduleClearTestingLayout")
                );
            if (clearButton)
            {
                foundClearButton = true;
                clearButton->click();
            }
            dialog->accept();
        });

        action->trigger();
        return foundDialog && foundScheduleTab && foundClearButton;
    }

    bool enableTestingAffectsM1ThroughPreferences()
    {
        QAction* const action = window
            ? window->findChild<QAction*>(QStringLiteral("preferencesAction"))
            : nullptr;
        if (!action)
        {
            return false;
        }

        QTimer::singleShot(0, window.get(), [this]()
        {
            auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                return;
            }
            foundPreferenceDialog = true;

            auto* const tabs = dialog->findChild<QTabWidget*>(
                QStringLiteral("preferencesTabs")
                );
            auto* const scheduleTab = dialog->findChild<QWidget*>(
                QStringLiteral("preferencesScheduleTab")
                );
            if (tabs && scheduleTab)
            {
                tabs->setCurrentWidget(scheduleTab);
                foundScheduleTab = true;
            }

            auto* const affectsM1 = dialog->findChild<QCheckBox*>(
                QStringLiteral("preferencesScheduleTestingAffectsM1")
                );
            if (affectsM1)
            {
                foundTestingAffectsM1CheckBox = true;
                testingAffectsM1CheckBoxWasInitiallyUnchecked =
                    !affectsM1->isChecked();
                affectsM1->click();
                testingAffectsM1CheckBoxEnabled = affectsM1->isChecked();
            }

            dialog->accept();
        });

        action->trigger();
        return foundPreferenceDialog
            && foundScheduleTab
            && foundTestingAffectsM1CheckBox
            && testingAffectsM1CheckBoxWasInitiallyUnchecked
            && testingAffectsM1CheckBoxEnabled;
    }

    bool seedRegularTestingPreferenceClasses()
    {
        return window && window->services()
            && seedRegularTestingPreferenceClassesInDatabase(
                *window->services(),
                &ids
                );
    }

    bool seed()
    {
        return window && window->services()
            && seedLayout(*window->services(), &ids);
    }

    int scheduleReadCount() const
    {
        DatabaseSession* const session = window && window->services()
            ? window->services()->databaseSession()
            : nullptr;
        TestingBlockRepository* const repository = session
            ? session->testingBlockRepository()
            : nullptr;
        return repository
            ? repository->testingAssignmentDisplayReadMetrics().callCount
            : -1;
    }

    static bool setTestingMode(
        SchedulePage* page,
        QString* failure
        )
    {
        if (!page)
        {
            if (failure)
            {
                *failure = QStringLiteral("Schedule page is null");
            }
            return false;
        }
        if (page->displayMode() == ScheduleDisplayMode::Testing)
        {
            return true;
        }
        QPushButton* const button = page->findChild<QPushButton*>(
            QStringLiteral("scheduleTestingModeButton")
            );
        if (!button)
        {
            if (failure)
            {
                *failure = QStringLiteral("testing-mode button is unavailable");
            }
            return false;
        }
        button->click();
        const bool selected =
            page->displayMode() == ScheduleDisplayMode::Testing;
        if (!selected && failure)
        {
            *failure = QStringLiteral("testing-mode click did not select Testing");
        }
        return selected;
    }

    static int testingAssignmentCount(SchedulePage* page)
    {
        ScheduleWidget* const widget = page
            ? page->findChild<ScheduleWidget*>()
            : nullptr;
        if (!widget)
        {
            return -1;
        }
        const ScheduleSummary summary = widget->scheduleModel().summary;
        return summary.testingBlocks + summary.testingClassBlocks;
    }

    static bool containsScheduleEntry(
        SchedulePage* page,
        const int classId
        )
    {
        ScheduleWidget* const widget = page
            ? page->findChild<ScheduleWidget*>()
            : nullptr;
        if (!widget)
        {
            return false;
        }
        const ScheduleViewModel model = widget->scheduleModel();
        for (const ScheduleRowView& row : model.rows)
        {
            for (const ScheduleCellView& cell : row.cells)
            {
                for (const ScheduleEntry& entry : cell.entries)
                {
                    if (
                        entry.classId == classId
                        && entry.kind == ScheduleEntryKind::RegularClass
                        )
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    QTemporaryDir directory;
    RecordingPromptService prompts;
    std::unique_ptr<MainWindow> window;
    SeedIds ids;
    MyWorkspacePage* workspace = nullptr;
    SchedulePage* workspaceSchedule = nullptr;
    SchedulePage* standaloneSchedule = nullptr;
    bool databaseReady = false;
    bool databaseFileReady = false;
    bool foundDialog = false;
    bool foundScheduleTab = false;
    bool foundClearButton = false;
    bool foundPreferenceDialog = false;
    bool foundTestingAffectsM1CheckBox = false;
    bool testingAffectsM1CheckBoxWasInitiallyUnchecked = false;
    bool testingAffectsM1CheckBoxEnabled = false;
    QString scheduleViewError;
};

void emitTranscript(const QString& json)
{
    qInfo().noquote() << QStringLiteral("F364_TRANSCRIPT=") + json;
}

void emitF378Transcript(const QString& json)
{
    qInfo().noquote() << QStringLiteral("F378_TRANSCRIPT=") + json;
}

QString jsonBool(const bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

} // namespace

class ScheduleTestingLayoutClearParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void warnsWhenNoDatabaseIsOpenBeforeConfirmation();
    void confirmationCancelLeavesRowsAndViewsUntouched();
    void writeFailureWarnsAndPreservesRowsWithoutRefresh();
    void successClearsLayoutPreservesSavedDataAndRefreshesViews();
    void preferencesToggleTestingGradeVisibilityAndRefreshesSchedule();
};

void ScheduleTestingLayoutClearParityTests::
warnsWhenNoDatabaseIsOpenBeforeConfirmation()
{
    LayoutClearFixture fixture(false);
    QVERIFY(fixture.window);
    QVERIFY(fixture.invokeClearPreferences());

    QCOMPARE(fixture.prompts.calls, QStringList{QStringLiteral("warning")});
    QCOMPARE(fixture.prompts.messages.size(), 1);
    QCOMPARE(fixture.prompts.messages.constFirst().title,
             QStringLiteral("Clear Testing Layout"));
    QCOMPARE(fixture.prompts.messages.constFirst().message,
             QStringLiteral("No Teacher Profile is open."));
    QVERIFY(fixture.prompts.confirmations.isEmpty());

    emitTranscript(QStringLiteral(
        "{\"case\":\"unavailable\",\"events\":[\"warning\"],"
        "\"warnings\":1,\"confirmations\":0}"
        ));
}

void ScheduleTestingLayoutClearParityTests::
confirmationCancelLeavesRowsAndViewsUntouched()
{
    LayoutClearFixture fixture(true);
    QVERIFY(fixture.databaseFileReady);
    QVERIFY(fixture.databaseReady);
    QVERIFY(fixture.seed());
    QVERIFY2(fixture.prepareScheduleViews(),
             qPrintable(fixture.scheduleViewError));
    QCOMPARE(fixture.scheduleReadCount() >= 2, true);
    const int readsBefore = fixture.scheduleReadCount();
    DatabaseSession* const session =
        fixture.window->services()->databaseSession();
    const QStringList expectedRows{
        QStringLiteral("Monday|16:00|Oral Room|-1"),
        QStringLiteral("Tuesday|16:00||%1").arg(fixture.ids.firstTestingClass),
        QStringLiteral("Wednesday|16:00||%1").arg(fixture.ids.secondTestingClass)
    };
    QCOMPARE(layoutRows(session->database()), expectedRows);
    fixture.prompts.choices.enqueue(PromptChoice::Rejected);

    QVERIFY(fixture.invokeClearPreferences());

    QCOMPARE(fixture.prompts.calls, QStringList{QStringLiteral("confirm")});
    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QVERIFY(fixture.prompts.messages.isEmpty());
    QCOMPARE(layoutRows(session->database()), expectedRows);
    QCOMPARE(fixture.scheduleReadCount(), readsBefore);

    emitTranscript(QStringLiteral(
        "{\"case\":\"cancel\",\"events\":[\"confirm\"],"
        "\"rows_before\":3,\"rows_after\":3,\"warnings\":0,"
        "\"schedule_refreshes\":0}"
        ));
}

void ScheduleTestingLayoutClearParityTests::
writeFailureWarnsAndPreservesRowsWithoutRefresh()
{
    LayoutClearFixture fixture(true);
    QVERIFY(fixture.databaseFileReady);
    QVERIFY(fixture.databaseReady);
    QVERIFY(fixture.seed());
    QVERIFY2(fixture.prepareScheduleViews(),
             qPrintable(fixture.scheduleViewError));
    DatabaseSession* const session =
        fixture.window->services()->databaseSession();
    const QStringList expectedRows{
        QStringLiteral("Monday|16:00|Oral Room|-1"),
        QStringLiteral("Tuesday|16:00||%1").arg(fixture.ids.firstTestingClass),
        QStringLiteral("Wednesday|16:00||%1").arg(fixture.ids.secondTestingClass)
    };
    QCOMPARE(layoutRows(session->database()), expectedRows);
    QVERIFY(executeSql(
        session->database(),
        QStringLiteral(
            "CREATE TRIGGER fail_testing_layout_clear "
            "BEFORE DELETE ON schedule_testing_blocks "
            "BEGIN SELECT RAISE(ABORT, 'F364 injected layout clear failure'); END"
            )
        ));
    const int readsBefore = fixture.scheduleReadCount();
    fixture.prompts.choices.enqueue(PromptChoice::Destructive);

    QVERIFY(fixture.invokeClearPreferences());

    QCOMPARE(fixture.prompts.calls,
             (QStringList{QStringLiteral("confirm"), QStringLiteral("warning")}));
    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QCOMPARE(fixture.prompts.messages.size(), 1);
    QCOMPARE(fixture.prompts.messages.constFirst().title,
             QStringLiteral("Clear Testing Layout"));
    QVERIFY(fixture.prompts.messages.constFirst().message.contains(
        QStringLiteral("F364 injected layout clear failure")
        ));
    QCOMPARE(layoutRows(session->database()), expectedRows);
    QCOMPARE(fixture.scheduleReadCount(), readsBefore);

    emitTranscript(QStringLiteral(
        "{\"case\":\"write_failure\",\"events\":[\"confirm\","
        "\"warning\"],\"rows_before\":3,\"rows_after\":3,"
        "\"warnings\":1,\"schedule_refreshes\":0}"
        ));
}

void ScheduleTestingLayoutClearParityTests::
successClearsLayoutPreservesSavedDataAndRefreshesViews()
{
    LayoutClearFixture fixture(true);
    QVERIFY(fixture.databaseFileReady);
    QVERIFY(fixture.databaseReady);
    QVERIFY(fixture.seed());
    QVERIFY2(fixture.prepareScheduleViews(),
             qPrintable(fixture.scheduleViewError));
    QVERIFY(fixture.workspaceSchedule);
    QVERIFY(fixture.standaloneSchedule);
    QVERIFY(fixture.workspaceSchedule->runtimeMetrics().modelEntryCount > 0);
    QVERIFY(fixture.standaloneSchedule->runtimeMetrics().modelEntryCount > 0);
    DatabaseSession* const session =
        fixture.window->services()->databaseSession();
    QSqlDatabase database = session->database();
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM schedule_testing_blocks")),
             3);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM testing_classes")),
             2);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM roster_columns")),
             3);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM roster_data")),
             3);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM class_times")),
             1);
    const int readsBefore = fixture.scheduleReadCount();
    fixture.prompts.choices.enqueue(PromptChoice::Destructive);

    QVERIFY(fixture.invokeClearPreferences());

    QCOMPARE(fixture.prompts.calls, QStringList{QStringLiteral("confirm")});
    QVERIFY(fixture.prompts.messages.isEmpty());
    QVERIFY(layoutRows(database).isEmpty());
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM testing_classes")),
             2);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM classes")),
             3);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM roster_columns")),
             3);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM roster_data")),
             3);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM class_info WHERE class_id=?"),
                    fixture.ids.unrelatedClass),
             1);
    QCOMPARE(scalar(database,
                    QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
                    fixture.ids.unrelatedClass),
             1);
    QVERIFY(fixture.scheduleReadCount() > readsBefore);
    QCOMPARE(LayoutClearFixture::testingAssignmentCount(
                 fixture.workspaceSchedule), 0);
    QCOMPARE(fixture.workspaceSchedule->runtimeMetrics().modelEntryCount, 1);
    QVERIFY(LayoutClearFixture::containsScheduleEntry(
        fixture.workspaceSchedule,
        fixture.ids.unrelatedClass
        ));
    QVERIFY(fixture.standaloneSchedule->needsRefresh());

    const int readsAfterWorkspaceRefresh = fixture.scheduleReadCount();
    fixture.window->pageManager()->showPage(PageType::Schedule);
    QApplication::processEvents();
    QCOMPARE(LayoutClearFixture::testingAssignmentCount(
                 fixture.standaloneSchedule), 0);
    QCOMPARE(fixture.standaloneSchedule->runtimeMetrics().modelEntryCount, 1);
    QVERIFY(LayoutClearFixture::containsScheduleEntry(
        fixture.standaloneSchedule,
        fixture.ids.unrelatedClass
        ));
    QVERIFY(!fixture.standaloneSchedule->needsRefresh());
    QVERIFY(fixture.scheduleReadCount() > readsAfterWorkspaceRefresh);

    emitTranscript(QStringLiteral(
        "{\"case\":\"success\",\"events\":[\"confirm\"],"
        "\"rows_before\":3,\"rows_after\":0,\"saved_testing_classes\":2,"
        "\"saved_roster_rows\":3,\"unrelated_class_info\":1,"
        "\"workspace_testing_assignments_after\":0,"
        "\"standalone_testing_assignments_after\":0,"
        "\"unrelated_schedule_entry_preserved\":true}"
        ));
}

void ScheduleTestingLayoutClearParityTests::
preferencesToggleTestingGradeVisibilityAndRefreshesSchedule()
{
    LayoutClearFixture fixture(true);
    QVERIFY(fixture.databaseFileReady);
    QVERIFY(fixture.databaseReady);
    QVERIFY(fixture.seedRegularTestingPreferenceClasses());
    QVERIFY2(fixture.prepareScheduleViews(),
             qPrintable(fixture.scheduleViewError));
    QVERIFY(fixture.workspaceSchedule);
    QVERIFY(fixture.workspaceSchedule->isVisible());

    ScheduleWidget* const widget =
        fixture.workspaceSchedule->findChild<ScheduleWidget*>();
    QVERIFY(widget);
    QCOMPARE(widget->displayState().displayMode,
             ScheduleDisplayMode::Testing);

    ClassMngr::Next::Platform::
        ApplicationServicesScheduleDisplayPreferencesPort preferencesPort(
            *fixture.window->services()
            );
    const auto initialPreferences = preferencesPort.load();
    QVERIFY(initialPreferences);
    QVERIFY(!initialPreferences.value().testingAffectsM1);
    QVERIFY(!widget->displayState().testingAffectsM1);

    const QSet<int> initiallyVisibleClasses = widget->visibleClassIds();
    QVERIFY(initiallyVisibleClasses.contains(fixture.ids.regularM1Class));
    QVERIFY(!initiallyVisibleClasses.contains(fixture.ids.regularM2Class));
    QCOMPARE(LayoutClearFixture::testingAssignmentCount(
                 fixture.workspaceSchedule), 0);

    auto* const banner = widget->findChild<QLabel*>(
        QStringLiteral("scheduleTestingBanner")
        );
    QVERIFY(banner);
    QCOMPARE(
        banner->text(),
        QStringLiteral(
            "Testing View — M2 and M3 classes are hidden; M1 classes remain"
            )
        );

    QVERIFY(fixture.enableTestingAffectsM1ThroughPreferences());

    const auto savedPreferences = preferencesPort.load();
    QVERIFY(savedPreferences);
    QVERIFY(savedPreferences.value().testingAffectsM1);
    QCOMPARE(widget->displayState().displayMode,
             ScheduleDisplayMode::Testing);
    QVERIFY(widget->displayState().testingAffectsM1);

    const QSet<int> visibleClassesAfterPreferenceChange =
        widget->visibleClassIds();
    QVERIFY(!visibleClassesAfterPreferenceChange.contains(
        fixture.ids.regularM1Class
        ));
    QVERIFY(!visibleClassesAfterPreferenceChange.contains(
        fixture.ids.regularM2Class
        ));
    QCOMPARE(LayoutClearFixture::testingAssignmentCount(
                 fixture.workspaceSchedule), 0);
    QCOMPARE(
        banner->text(),
        QStringLiteral("Testing View — M1, M2, and M3 classes are hidden")
        );

    emitF378Transcript(QStringLiteral(
        "{\"case\":\"testing_grade_preference\","
        "\"before\":{\"m1_visible\":true,\"m2_visible\":false,"
        "\"testing_affects_m1\":false,"
        "\"banner\":\"m2_m3_hidden_m1_remains\"},"
        "\"after\":{\"m1_visible\":false,\"m2_visible\":false,"
        "\"testing_affects_m1\":true,\"preference_persisted\":true,"
        "\"banner\":\"m1_m2_m3_hidden\"}}"
        ));
}

QTEST_MAIN(ScheduleTestingLayoutClearParityTests)

#include "schedule_testing_layout_clear_parity_tests.moc"

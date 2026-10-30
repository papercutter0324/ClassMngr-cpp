#include "data/database/workspace_store.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/testing_block_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "domain/models/testing_class.h"
#include "next/application/testing_class_create.h"
#include "next/platform/workspace_store_testing_classes_actions.h"
#include "next/platform/workspace_store_roster_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/roster_editor_widget.h"
#include "next/ui/roster_table_model.h"
#include "next/ui/testing_classes_manager_page.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QFile>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTableView>
#include <QtTest/QtTest>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

[[nodiscard]] Domain::ClassId typedClassId(const int id)
{
    return *Domain::ClassId::fromString(std::to_string(id));
}

[[nodiscard]] Application::TestingClassCreateRequest createRequest(
    const QString& name,
    std::optional<Application::TestingClassPendingScheduleSlot> slot = {},
    std::optional<Domain::TeacherId> teacherId = {}
    )
{
    return {
        .name = name.toStdU16String(),
        .grade = u"M1",
        .level = u"Major",
        .room = u"Room 1",
        .teacherId = std::move(teacherId),
        .classColor = u"#FFFFFF",
        .fontColor = u"#000000",
        .notes = u"Testing notes",
        .pendingScheduleSlot = std::move(slot)
    };
}

[[nodiscard]] int seedTestingClass(
    WorkspaceStore& store,
    const QString& name,
    const QString& grade = QStringLiteral("M1"),
    const QString& level = QStringLiteral("Major"),
    const QString& room = QStringLiteral("Room 1"),
    const qint64 teacherId = -1,
    const QString& notes = QStringLiteral("Initial notes")
    )
{
    TestingClass testingClass;
    testingClass.name = name;
    testingClass.grade = grade;
    testingClass.level = level;
    testingClass.room = room;
    testingClass.teacherId = teacherId;
    testingClass.classColor = QStringLiteral("#FFFFFF");
    testingClass.fontColor = QStringLiteral("#000000");
    testingClass.notes = notes;
    const auto created = store.testingClassRepository()->createTestingClass(
        testingClass);
    return created ? *created : -1;
}

[[nodiscard]] bool executeSql(WorkspaceStore& store, const QString& sql)
{
    QSqlQuery query(store.database());
    return query.exec(sql);
}

[[nodiscard]] int countBySql(WorkspaceStore& store, const QString& sql)
{
    QSqlQuery query(store.database());
    if (!query.exec(sql) || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

[[nodiscard]] bool insertCascadeRows(
    WorkspaceStore& store,
    const int classId,
    const QString& day,
    const QString& evaluationName
    )
{
    QSqlQuery query(store.database());
    query.prepare(QStringLiteral(
        "INSERT INTO schedule_testing_blocks (day,start_time,room,class_id) "
        "VALUES (?, '14:00', 'Room', ?)"));
    query.addBindValue(day);
    query.addBindValue(classId);
    if (!query.exec()) return false;

    query.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id,name,position,width) "
        "VALUES (?, 'English', 0, 120)"));
    query.addBindValue(classId);
    if (!query.exec()) return false;
    query.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id,row_index,col_index,value) "
        "VALUES (?, 0, 0, 'Student')"));
    query.addBindValue(classId);
    if (!query.exec()) return false;

    query.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id,evaluation_name) "
        "VALUES (?, ?)"));
    query.addBindValue(classId);
    query.addBindValue(evaluationName);
    if (!query.exec()) return false;
    const QVariant evaluationId = query.lastInsertId();
    query.prepare(QStringLiteral(
        "INSERT INTO speaking_eval_data (evaluation_id,row_index) VALUES (?, 0)"));
    query.addBindValue(evaluationId);
    if (!query.exec()) return false;

    query.prepare(QStringLiteral("INSERT INTO class_times (class_id) VALUES (?)"));
    query.addBindValue(classId);
    if (!query.exec()) return false;
    query.prepare(QStringLiteral(
        "INSERT INTO class_intensive_times (class_id) VALUES (?)"));
    query.addBindValue(classId);
    return query.exec();
}

using CascadeCounts = std::array<int, 10>;

[[nodiscard]] CascadeCounts cascadeCounts(WorkspaceStore& store, const int id)
{
    const QStringList statements{
        QStringLiteral("SELECT COUNT(*) FROM classes WHERE id=?"),
        QStringLiteral("SELECT COUNT(*) FROM class_info WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM testing_classes WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM schedule_testing_blocks WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM roster_columns WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM roster_data WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM speaking_evaluations WHERE class_id=?"),
        QStringLiteral(
            "SELECT COUNT(*) FROM speaking_eval_data WHERE evaluation_id IN ("
            "SELECT id FROM speaking_evaluations WHERE class_id=?)"),
        QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
        QStringLiteral("SELECT COUNT(*) FROM class_intensive_times WHERE class_id=?")
    };
    CascadeCounts result{};
    for (qsizetype index = 0; index < statements.size(); ++index)
    {
        QSqlQuery query(store.database());
        query.prepare(statements.at(index));
        query.addBindValue(id);
        if (!query.exec() || !query.next())
        {
            result[static_cast<std::size_t>(index)] = -1;
        }
        else
        {
            result[static_cast<std::size_t>(index)] = query.value(0).toInt();
        }
    }
    return result;
}

class EmptyTestingPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

struct PageHarness final
{
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    QString path;
    Application::SaveMode saveMode = Application::SaveMode::Manual;
    Ui::TestingClassesUnsavedChoice unsavedChoice =
        Ui::TestingClassesUnsavedChoice::Cancel;
    bool confirmDeletion = true;
    bool failNextDetailsRead = false;
    bool failRosterSave = false;
    QString lastDeletionPrompt;
    int rosterReadCount = 0;
    std::vector<QString> rosterReadPaths;

    [[nodiscard]] bool initialize()
    {
        const auto loaded = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
        if (!loaded)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(std::move(*loaded));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);
        Ui::PageDescriptor manager{
            .id = *Ui::PageId::fromString(QStringLiteral("testing_classes")),
            .label = QStringLiteral("Testing Classes"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::TestingClassesManagerPageActions actions;
                actions.saveMode = saveMode;
                actions.workspaceIdentity = [this] { return path; };
                actions.activeWorkspacePath = [this]() -> std::optional<QString> {
                    return path.isEmpty() ? std::nullopt
                                          : std::optional<QString>(path);
                };
                actions.readClasses = [](const QString& activePath) {
                    return Platform::readTestingClassesAtPath(activePath);
                };
                actions.readDetails = [this](
                    const QString& activePath,
                    const Domain::ClassId& classId) {
                    if (failNextDetailsRead)
                    {
                        failNextDetailsRead = false;
                        return Application::TestingClassDetailsReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "temporary testing details read failure",
                            .recoverable = true
                        });
                    }
                    return Platform::readTestingClassDetailsAtPath(
                        activePath, classId);
                };
                actions.readTeacherChoices = [](
                    const QString& activePath) {
                    return Platform::readTestingTeacherChoicesAtPath(activePath);
                };
                actions.updateDetails = [](
                    const QString& activePath,
                    const Application::TestingClassDetailsUpdateRequest& request) {
                    return Platform::updateTestingClassDetailsAtPath(
                        activePath, request);
                };
                actions.createClass = [](
                    const QString& activePath,
                    const Application::TestingClassCreateRequest& request) {
                    return Platform::createTestingClassAtPath(activePath, request);
                };
                actions.deleteClass = [](
                    const QString& activePath,
                    const Application::TestingClassDeleteRequest& request) {
                    return Platform::deleteTestingClassAtPath(activePath, request);
                };
                actions.rosterEditor.readRoster = [this](
                    const QString& activePath,
                    const Domain::ClassId& classId) {
                    ++rosterReadCount;
                    rosterReadPaths.push_back(activePath);
                    return Platform::readRosterWindowAtPath(activePath, classId);
                };
                actions.rosterEditor.saveRoster = [this](
                    const QString& activePath,
                    const Application::RosterSaveRequest& request) {
                    if (failRosterSave)
                    {
                        return Domain::Result<void>::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "injected testing roster save failure",
                            .recoverable = true
                        });
                    }
                    return Platform::saveRosterAtPath(activePath, request);
                };
                actions.rosterEditor.columnHasTailData =
                    Platform::rosterColumnHasTailDataAtPath;
                actions.resolveUnsavedChanges = [this](const QString&) {
                    return unsavedChoice;
                };
                actions.confirmDelete = [this](const QString& prompt) {
                    lastDeletionPrompt = prompt;
                    return confirmDeletion;
                };
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::TestingClassesManagerPage>(
                        context, std::move(actions)));
            }
        };
        Ui::PageDescriptor other{
            .id = *Ui::PageId::fromString(QStringLiteral("other")),
            .label = QStringLiteral("Other"),
            .factory = [](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<EmptyTestingPage>(context));
            }
        };
        return host->registerPage(std::move(manager))
            && host->registerPage(std::move(other));
    }

    [[nodiscard]] Ui::TestingClassesManagerPage* page() const
    {
        return dynamic_cast<Ui::TestingClassesManagerPage*>(
            host ? host->pageInstance(QStringLiteral("testing_classes")) : nullptr);
    }

    [[nodiscard]] bool activate()
    {
        if (!host || !host->activatePage(QStringLiteral("testing_classes")))
        {
            return false;
        }
        host->resize(1024, 768);
        host->show();
        QApplication::processEvents();
        return true;
    }
};

void userReplace(QLineEdit* edit, const QString& text)
{
    edit->setFocus();
    edit->selectAll();
    QTest::keyClicks(edit, text);
}

int widgetCount(const QWidget* widget)
{
    return widget ? widget->findChildren<QWidget*>().size() + 1 : 0;
}

} // namespace

class TestingClassesManagerTests final : public QObject
{
    Q_OBJECT

private slots:
    void pathActionsCreatePendingSlotAndRollbackOccupiedSlot();
    void pathActionsPreserveDetailsNotesAndWideTeacherIdentity();
    void pathDeleteCascadesAndRollbackKeepsSiblingAndOrdinaryClass();
    void newClassCreateFailureRollsBackAndCanRetry();
    void visiblePageSaveDiscardCancelAndSelectionRecovery();
    void visiblePageAddDeleteLifecycle();
    void manualAutomaticAndPendingCreateSaveBehavior();
    void visiblePageFailedSaveRetainsDraftAndRetrySucceeds();
    void readRetryAndWorkspaceLifecycleClearSnapshots();
    void rosterEntryIsLazyUsesTestingModeAndActiveWorkspacePath();
    void rosterSectionSuspensionReleasesTestingModel();
    void rosterSelectionAndPageChangesReleaseTestingModel();
    void rosterSectionExitResolvesDraftBeforeClearingAndReloads();
    void rosterDirtyChoicesSaveDiscardAndCancelBeforeClassSelection();
    void ninetySixTestingClassesUseOneEditorTree();
};

void TestingClassesManagerTests::
pathActionsCreatePendingSlotAndRollbackOccupiedSlot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-slot.tps"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
    }

    const Application::TestingClassPendingScheduleSlot slot{
        .weekday = Domain::Weekday::Thursday,
        .startMinute = Domain::ScheduleTime::StartMinute{10 * 60 + 35}
    };
    const auto created = Platform::createTestingClassAtPath(
        path, createRequest(QStringLiteral("Assigned Atomic Class"), slot));
    QVERIFY(created);
    const Domain::ClassId createdId = created.value();

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery query(store.database());
        query.prepare(QStringLiteral(
            "SELECT day,start_time,class_id FROM schedule_testing_blocks "
            "WHERE class_id=?"));
        query.addBindValue(QString::fromStdString(createdId.value()).toInt());
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("Thursday"));
        QCOMPARE(query.value(1).toString(), QStringLiteral("10:35"));
        QCOMPARE(query.value(2).toInt(),
            QString::fromStdString(createdId.value()).toInt());
        QVERIFY(!query.next());
        QVERIFY(store.testingBlockRepository()->saveTestingBlock(
            QStringLiteral("Tuesday"), QStringLiteral("11:30"),
            QStringLiteral("Occupied block")));
    }

    const auto before = Platform::readTestingClassesAtPath(path);
    QVERIFY(before);
    const Application::TestingClassPendingScheduleSlot occupied{
        .weekday = Domain::Weekday::Tuesday,
        .startMinute = Domain::ScheduleTime::StartMinute{11 * 60 + 30}
    };
    const auto rejected = Platform::createTestingClassAtPath(
        path, createRequest(QStringLiteral("Must Roll Back Class"), occupied));
    QVERIFY(!rejected);
    QCOMPARE(rejected.error().code, Domain::ErrorCode::Conflict);

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QCOMPARE(countBySql(store,
        QStringLiteral("SELECT COUNT(*) FROM classes WHERE name='Must Roll Back Class'")), 0);
    QCOMPARE(countBySql(store,
        QStringLiteral("SELECT COUNT(*) FROM class_info WHERE class_id IN "
            "(SELECT id FROM classes WHERE name='Must Roll Back Class')")), 0);
    QCOMPARE(countBySql(store,
        QStringLiteral("SELECT COUNT(*) FROM testing_classes WHERE class_id IN "
            "(SELECT id FROM classes WHERE name='Must Roll Back Class')")), 0);
    QCOMPARE(countBySql(store,
        QStringLiteral("SELECT COUNT(*) FROM schedule_testing_blocks")), 2);
    const auto after = Platform::readTestingClassesAtPath(path);
    QVERIFY(after);
    QCOMPARE(after.value().choices.size(), before.value().choices.size());
}

void TestingClassesManagerTests::
pathActionsPreserveDetailsNotesAndWideTeacherIdentity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-details.tps"));
    constexpr qint64 wideTeacherId = 4'000'000'007LL;
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        QSqlQuery teacher(store.database());
        teacher.prepare(QStringLiteral(
            "INSERT INTO teachers (id,teacher_kr,teacher_en,room_number) "
            "VALUES (?, '테스트 교사', 'Test Teacher', 'K-304')"));
        teacher.addBindValue(wideTeacherId);
        QVERIFY(teacher.exec());
    }
    const auto teacherId = Domain::TeacherId::fromString(
        std::to_string(wideTeacherId));
    QVERIFY(teacherId);
    const auto created = Platform::createTestingClassAtPath(
        path, createRequest(QStringLiteral("Original Testing Details"), {}, teacherId));
    QVERIFY(created);

    const auto teacherChoices = Platform::readTestingTeacherChoicesAtPath(path);
    QVERIFY(teacherChoices);
    const auto selectedTeacher = std::find_if(
        teacherChoices.value().choices.cbegin(),
        teacherChoices.value().choices.cend(),
        [teacherId](const auto& choice) { return choice.teacherId == *teacherId; });
    QVERIFY(selectedTeacher != teacherChoices.value().choices.cend());
    QCOMPARE(selectedTeacher->name, std::u16string(u"테스트 교사"));
    const auto details = Platform::readTestingClassDetailsAtPath(
        path, created.value());
    QVERIFY(details);
    QCOMPARE(details.value().teacherId, teacherId);
    QCOMPARE(details.value().notes, std::u16string(u"Testing notes"));

    Application::TestingClassDetailsUpdateRequest rename{
        .classId = created.value(),
        .name = u"Renamed Testing Details",
        .grade = u"M1",
        .level = u"Major",
        .room = u"Room 1",
        .teacherId = teacherId,
        .classColor = u"#FFFFFF",
        .fontColor = u"#000000",
        .notes = u"Testing notes"
    };
    QVERIFY(Platform::updateTestingClassDetailsAtPath(path, rename));
    auto saved = Platform::readTestingClassDetailsAtPath(path, created.value());
    QVERIFY(saved);
    QCOMPARE(saved.value().name, std::u16string(u"Renamed Testing Details"));
    QCOMPARE(saved.value().notes, std::u16string(u"Testing notes"));
    QCOMPARE(saved.value().teacherId, teacherId);

    rename.name = saved.value().name;
    rename.room = u"Room 2";
    rename.notes = u"Updated notes";
    QVERIFY(Platform::updateTestingClassDetailsAtPath(path, rename));
    saved = Platform::readTestingClassDetailsAtPath(path, created.value());
    QVERIFY(saved);
    QCOMPARE(saved.value().room, std::u16string(u"Room 2"));
    QCOMPARE(saved.value().notes, std::u16string(u"Updated notes"));
    QCOMPARE(saved.value().teacherId, teacherId);
}

void TestingClassesManagerTests::
pathDeleteCascadesAndRollbackKeepsSiblingAndOrdinaryClass()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-delete.tps"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
    }
    const auto target = Platform::createTestingClassAtPath(
        path, createRequest(QStringLiteral("Delete Target")));
    const auto sibling = Platform::createTestingClassAtPath(
        path, createRequest(QStringLiteral("Keep Sibling")));
    QVERIFY(target);
    QVERIFY(sibling);
    int ordinaryClassId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        const auto ordinary = store.classRepository()->createClass(
            QStringLiteral("Ordinary Class"));
        QVERIFY(ordinary);
        ordinaryClassId = ordinary.value();
        QVERIFY(insertCascadeRows(store,
            QString::fromStdString(target.value().value()).toInt(),
            QStringLiteral("Monday"), QStringLiteral("Target Winter")));
        QVERIFY(insertCascadeRows(store,
            QString::fromStdString(sibling.value().value()).toInt(),
            QStringLiteral("Wednesday"), QStringLiteral("Sibling Winter")));
        QVERIFY(cascadeCounts(store,
            QString::fromStdString(target.value().value()).toInt())
            == CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
    }

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        const QString trigger = QStringLiteral(
            "CREATE TRIGGER fail_testing_class_final_delete "
            "BEFORE DELETE ON classes WHEN OLD.id=%1 BEGIN "
            "SELECT RAISE(ABORT,'blocked'); END")
            .arg(QString::fromStdString(target.value().value()));
        QVERIFY(executeSql(store, trigger));
    }
    const auto rejected = Platform::deleteTestingClassAtPath(path,
        {.classId = target.value()});
    QVERIFY(!rejected);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QVERIFY(cascadeCounts(store,
            QString::fromStdString(target.value().value()).toInt())
            == CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
        QVERIFY(cascadeCounts(store,
            QString::fromStdString(sibling.value().value()).toInt())
            == CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
        QVERIFY(executeSql(store, QStringLiteral(
            "DROP TRIGGER fail_testing_class_final_delete")));
    }
    QVERIFY(Platform::deleteTestingClassAtPath(path,
        {.classId = target.value()}));

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QVERIFY(cascadeCounts(store,
        QString::fromStdString(target.value().value()).toInt())
        == CascadeCounts({0,0,0,0,0,0,0,0,0,0}));
    QVERIFY(cascadeCounts(store,
        QString::fromStdString(sibling.value().value()).toInt())
        == CascadeCounts({1,1,1,1,1,1,1,1,1,1}));
    const auto ordinary = store.classRepository()->getClasses();
    QVERIFY(ordinary);
    QCOMPARE(ordinary->size(), 1);
    QCOMPARE(ordinary->first().id, ordinaryClassId);
}

void TestingClassesManagerTests::newClassCreateFailureRollsBackAndCanRetry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-create-rollback.tps"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        QVERIFY(executeSql(store, QStringLiteral(
            "CREATE TRIGGER fail_testing_insert BEFORE INSERT ON testing_classes "
            "BEGIN SELECT RAISE(ABORT,'blocked'); END")));
    }
    const auto rejected = Platform::createTestingClassAtPath(
        path, createRequest(QStringLiteral("Rollback Then Retry")));
    QVERIFY(!rejected);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QCOMPARE(countBySql(store, QStringLiteral(
            "SELECT COUNT(*) FROM classes WHERE name='Rollback Then Retry'")), 0);
        QCOMPARE(countBySql(store, QStringLiteral(
            "SELECT COUNT(*) FROM class_info WHERE class_id IN "
            "(SELECT id FROM classes WHERE name='Rollback Then Retry')")), 0);
        QCOMPARE(countBySql(store, QStringLiteral(
            "SELECT COUNT(*) FROM testing_classes")), 0);
        QVERIFY(executeSql(store, QStringLiteral(
            "DROP TRIGGER fail_testing_insert")));
    }
    const auto retried = Platform::createTestingClassAtPath(
        path, createRequest(QStringLiteral("Rollback Then Retry")));
    QVERIFY(retried);
    const auto details = Platform::readTestingClassDetailsAtPath(path, retried.value());
    QVERIFY(details);
    QCOMPARE(details.value().name, std::u16string(u"Rollback Then Retry"));
}

void TestingClassesManagerTests::
visiblePageSaveDiscardCancelAndSelectionRecovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-page-choices.tps"));
    int alphaId = -1;
    int betaId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        alphaId = seedTestingClass(store, QStringLiteral("Alpha"));
        betaId = seedTestingClass(store, QStringLiteral("Beta"));
    }
    QVERIFY(alphaId > 0);
    QVERIFY(betaId > 0);

    PageHarness harness;
    harness.path = path;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    Ui::TestingClassesManagerPage* const page = harness.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 2);
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(typedClassId(alphaId)));

    userReplace(page->nameEdit(), QStringLiteral("Alpha Draft"));
    QVERIFY(page->hasUnsavedChanges());
    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Cancel;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(betaId)));
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(typedClassId(alphaId)));
    QCOMPARE(page->nameEdit()->text(), QStringLiteral("Alpha Draft"));

    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Discard;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(betaId)));
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(typedClassId(betaId)));
    QCOMPARE(page->nameEdit()->text(), QStringLiteral("Beta"));
    QVERIFY(!page->hasUnsavedChanges());

    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(alphaId)));
    userReplace(page->nameEdit(), QStringLiteral("Alpha Saved"));
    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Save;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(betaId)));
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(typedClassId(betaId)));
    const auto alpha = Platform::readTestingClassDetailsAtPath(
        path, typedClassId(alphaId));
    QVERIFY(alpha);
    QCOMPARE(alpha.value().name, std::u16string(u"Alpha Saved"));
}

void TestingClassesManagerTests::visiblePageAddDeleteLifecycle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-add-delete.tps"));
    int firstId = -1;
    int secondId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        firstId = seedTestingClass(store, QStringLiteral("First"));
        secondId = seedTestingClass(store, QStringLiteral("Second"));
    }

    PageHarness harness;
    harness.path = path;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* page = harness.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 2);
    page->addButton()->click();
    QVERIFY(page->isNewDraft());
    QVERIFY(!page->selectedClassId());
    userReplace(page->nameEdit(), QStringLiteral("Page Created"));
    userReplace(page->roomEdit(), QStringLiteral("Page Room"));
    page->saveButton()->click();
    QVERIFY(!page->isNewDraft());
    QVERIFY(page->selectedClassId());
    QVERIFY(page->hasLoadedDetails());
    QCOMPARE(page->model()->classCount(), 3);
    const Domain::ClassId createdId = *page->selectedClassId();
    QCOMPARE(page->nameEdit()->text(), QStringLiteral("Page Created"));

    harness.confirmDeletion = false;
    page->deleteButton()->click();
    QCOMPARE(page->model()->classCount(), 3);
    QVERIFY(Platform::readTestingClassDetailsAtPath(path, createdId));

    harness.confirmDeletion = true;
    page->deleteButton()->click();
    QCOMPARE(page->model()->classCount(), 2);
    QVERIFY(!page->selectedClassId()
        || *page->selectedClassId() != createdId);
    QVERIFY(!Platform::readTestingClassDetailsAtPath(path, createdId));
    QVERIFY(harness.lastDeletionPrompt.contains(QStringLiteral("roster"),
        Qt::CaseInsensitive));
    QVERIFY(harness.lastDeletionPrompt.contains(
        QStringLiteral("Testing schedule assignments"), Qt::CaseInsensitive));
    const auto remaining = Platform::readTestingClassesAtPath(path);
    QVERIFY(remaining);
    QCOMPARE(remaining.value().choices.size(), 2);
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);
}

void TestingClassesManagerTests::
manualAutomaticAndPendingCreateSaveBehavior()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-save-modes.tps"));
    int existingId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        existingId = seedTestingClass(store, QStringLiteral("Existing"));
    }

    PageHarness manual;
    manual.path = path;
    manual.saveMode = Application::SaveMode::Manual;
    QVERIFY(manual.initialize());
    QVERIFY(manual.activate());
    auto* page = manual.page();
    QVERIFY(page);
    userReplace(page->nameEdit(), QStringLiteral("Manual Draft"));
    QTest::qWait(850);
    auto persisted = Platform::readTestingClassDetailsAtPath(
        path, typedClassId(existingId));
    QVERIFY(persisted);
    QCOMPARE(persisted.value().name, std::u16string(u"Existing"));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(page->saveButton()->isEnabled());
    page->saveButton()->click();
    persisted = Platform::readTestingClassDetailsAtPath(
        path, typedClassId(existingId));
    QVERIFY(persisted);
    QCOMPARE(persisted.value().name, std::u16string(u"Manual Draft"));

    const int beforeCreate = Platform::readTestingClassesAtPath(path)
        .value().choices.size();
    const Application::TestingClassPendingScheduleSlot slot{
        .weekday = Domain::Weekday::Thursday,
        .startMinute = Domain::ScheduleTime::StartMinute{10 * 60}
    };
    page->openNewClassForPendingSlot(slot);
    QVERIFY(page->pendingSlotLabel()->isHidden());
    QVERIFY(page->pendingSlotLabel()->text().isEmpty());
    userReplace(page->nameEdit(), QStringLiteral("Pending Slot Class"));
    userReplace(page->roomEdit(), QStringLiteral("Testing Room"));
    QTest::qWait(850);
    QCOMPARE(Platform::readTestingClassesAtPath(path).value().choices.size(),
        beforeCreate);
    QVERIFY(page->isNewDraft());
    QVERIFY(page->hasUnsavedChanges());
    page->saveButton()->click();
    QVERIFY(!page->isNewDraft());
    QVERIFY(page->selectedClassId());
    const auto createdDetails = Platform::readTestingClassDetailsAtPath(
        path, *page->selectedClassId());
    QVERIFY(createdDetails);
    QCOMPARE(createdDetails.value().name, std::u16string(u"Pending Slot Class"));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery assignment(store.database());
        assignment.prepare(QStringLiteral(
            "SELECT day,start_time,class_id FROM schedule_testing_blocks "
            "WHERE class_id=?"));
        assignment.addBindValue(QString::fromStdString(
            page->selectedClassId()->value()).toInt());
        QVERIFY(assignment.exec());
        QVERIFY(assignment.next());
        QCOMPARE(assignment.value(0).toString(), QStringLiteral("Thursday"));
        QCOMPARE(assignment.value(1).toString(), QStringLiteral("10:00"));
        QCOMPARE(assignment.value(2).toInt(), QString::fromStdString(
            page->selectedClassId()->value()).toInt());
    }

    PageHarness automatic;
    automatic.path = path;
    automatic.saveMode = Application::SaveMode::Automatic;
    QVERIFY(automatic.initialize());
    QVERIFY(automatic.activate());
    auto* automaticPage = automatic.page();
    QVERIFY(automaticPage);
    const Domain::ClassId selected = *automaticPage->selectedClassId();
    userReplace(automaticPage->nameEdit(), QStringLiteral("Automatic Saved"));
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        const auto saved = Platform::readTestingClassDetailsAtPath(path, selected);
        return saved && saved.value().name == std::u16string(u"Automatic Saved");
    }(), 2'500);
    QVERIFY(!automaticPage->hasUnsavedChanges());
}

void TestingClassesManagerTests::
visiblePageFailedSaveRetainsDraftAndRetrySucceeds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-page-retry.tps"));
    int alphaId = -1;
    int betaId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        alphaId = seedTestingClass(store, QStringLiteral("Alpha"));
        betaId = seedTestingClass(store, QStringLiteral("Beta"));
        QVERIFY(executeSql(store, QStringLiteral(
            "CREATE TRIGGER fail_testing_class_update "
            "BEFORE UPDATE ON testing_classes BEGIN "
            "SELECT RAISE(ABORT,'blocked'); END")));
    }

    PageHarness harness;
    harness.path = path;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* page = harness.page();
    QVERIFY(page);
    userReplace(page->roomEdit(), QStringLiteral("Unsaved Room"));
    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Save;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(betaId)));
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(typedClassId(alphaId)));
    QCOMPARE(page->roomEdit()->text(), QStringLiteral("Unsaved Room"));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(page->statusLabel()->isVisible());

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QVERIFY(executeSql(store, QStringLiteral(
            "DROP TRIGGER fail_testing_class_update")));
    }
    page->saveButton()->click();
    QVERIFY(!page->hasUnsavedChanges());
    const auto saved = Platform::readTestingClassDetailsAtPath(
        path, typedClassId(alphaId));
    QVERIFY(saved);
    QCOMPARE(saved.value().room, std::u16string(u"Unsaved Room"));
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(typedClassId(alphaId)));
}

void TestingClassesManagerTests::
readRetryAndWorkspaceLifecycleClearSnapshots()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("testing-a.tps"));
    const QString secondPath = directory.filePath(QStringLiteral("testing-b.tps"));
    int firstId = -1;
    int secondId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.open(firstPath));
        firstId = seedTestingClass(store, QStringLiteral("Workspace A"));
    }
    {
        WorkspaceStore store;
        QVERIFY(store.open(secondPath));
        secondId = seedTestingClass(store, QStringLiteral("Workspace B"));
    }

    PageHarness harness;
    harness.path = firstPath;
    harness.failNextDetailsRead = true;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* page = harness.page();
    QVERIFY(page);
    QVERIFY(!page->hasLoadedDetails());
    QVERIFY(page->statusLabel()->isVisible());
    page->retryButton()->click();
    QVERIFY(page->hasLoadedDetails());
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(typedClassId(firstId)));

    QVERIFY(harness.host->activatePage(QStringLiteral("other")));
    QVERIFY(!page->hasLoadedDetails());
    QCOMPARE(page->nameEdit()->text(), QString{});
    harness.path = secondPath;
    page->clearForWorkspaceChange();
    QVERIFY(harness.host->activatePage(QStringLiteral("testing_classes")));
    QCOMPARE(page->selectedClassId(),
        std::optional<Domain::ClassId>(typedClassId(secondId)));
    QCOMPARE(page->nameEdit()->text(), QStringLiteral("Workspace B"));
    QVERIFY(page->hasLoadedDetails());

    QVERIFY(harness.host->activatePage(QStringLiteral("other")));
    harness.path.clear();
    page->clearForClosedWorkspace();
    QVERIFY(harness.host->activatePage(QStringLiteral("testing_classes")));
    QVERIFY(!page->selectedClassId());
    QVERIFY(!page->hasLoadedDetails());
    QCOMPARE(page->model()->classCount(), 0);
}

void TestingClassesManagerTests::
rosterEntryIsLazyUsesTestingModeAndActiveWorkspacePath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-roster.tps"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        QVERIFY(seedTestingClass(store, QStringLiteral("Testing roster")) > 0);
    }

    PageHarness harness;
    harness.path = path;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    Ui::TestingClassesManagerPage* const page = harness.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 1);
    QVERIFY(page->selectedClassId());
    QCOMPARE(harness.rosterReadCount, 0);
    QVERIFY(!page->rosterEditor()->isLoaded());

    page->sections()->setCurrentIndex(2);
    QCOMPARE(harness.rosterReadCount, 1);
    QCOMPARE(harness.rosterReadPaths, (std::vector<QString>{path}));
    QVERIFY(page->rosterEditor()->isLoaded());
    QVERIFY(page->rosterEditor()->tableView()->isColumnHidden(2));
    QVERIFY(!page->rosterEditor()->tableView()->isColumnHidden(0));
}

void TestingClassesManagerTests::rosterSectionSuspensionReleasesTestingModel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-section.tps"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        QVERIFY(seedTestingClass(store, QStringLiteral("Testing section")) > 0);
    }

    PageHarness harness;
    harness.path = path;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    page->sections()->setCurrentIndex(2);
    QVERIFY(page->rosterEditor()->isLoaded());

    page->sections()->setCurrentIndex(0);
    QVERIFY(!page->rosterEditor()->isLoaded());
    QCOMPARE(page->rosterEditor()->model()->rowCount(), 25);
    QCOMPARE(page->rosterEditor()->model()->columnCount(), 6);
}

void TestingClassesManagerTests::rosterSelectionAndPageChangesReleaseTestingModel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-lifecycle.tps"));
    int firstId = -1;
    int secondId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        firstId = seedTestingClass(store, QStringLiteral("First testing class"));
        secondId = seedTestingClass(store, QStringLiteral("Second testing class"));
        QVERIFY(firstId > 0);
        QVERIFY(secondId > firstId);
        const auto seedRosterName = [&store](const int classId, const QString& name) {
            const QStringList columns{
                QStringLiteral("English"), QStringLiteral("Korean"),
                QStringLiteral("Winter"), QStringLiteral("Speech Contest"),
                QStringLiteral("Summer"), QStringLiteral("Fall")};
            for (int index = 0; index < columns.size(); ++index)
            {
                QSqlQuery insertColumn(store.database());
                insertColumn.prepare(QStringLiteral(
                    "INSERT INTO roster_columns(class_id,name,position,width) "
                    "VALUES (?,?,?,120)"));
                insertColumn.addBindValue(classId);
                insertColumn.addBindValue(columns.at(index));
                insertColumn.addBindValue(index);
                if (!insertColumn.exec()) return false;
            }
            QSqlQuery insertCell(store.database());
            insertCell.prepare(QStringLiteral(
                "INSERT INTO roster_data(class_id,row_index,col_index,value) "
                "VALUES (?,0,0,?)"));
            insertCell.addBindValue(classId);
            insertCell.addBindValue(name);
            return insertCell.exec();
        };
        QVERIFY(seedRosterName(firstId, QStringLiteral("First roster")));
        QVERIFY(seedRosterName(secondId, QStringLiteral("Second roster")));
    }

    PageHarness harness;
    harness.path = path;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    QCOMPARE(page->selectedClassId()->value(), std::to_string(firstId));
    page->sections()->setCurrentIndex(2);
    QVERIFY(page->rosterEditor()->isLoaded());
    QCOMPARE(harness.rosterReadCount, 1);

    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(secondId)));
    QCOMPARE(page->selectedClassId()->value(), std::to_string(secondId));
    QCOMPARE(harness.rosterReadCount, 2);
    QVERIFY(page->rosterEditor()->isLoaded());
    QCOMPARE(page->rosterEditor()->model()->data(
        page->rosterEditor()->model()->index(0, 0)).toString(),
        QStringLiteral("Second roster"));
    QVERIFY(harness.host->activatePage(QStringLiteral("other")));
    QVERIFY(!page->rosterEditor()->isLoaded());
}

void TestingClassesManagerTests::rosterSectionExitResolvesDraftBeforeClearingAndReloads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-roster-section-exit.tps"));
    int classId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        classId = seedTestingClass(store, QStringLiteral("Section exit class"));
        QVERIFY(classId > 0);
        const QStringList columns{
            QStringLiteral("English"), QStringLiteral("Korean"),
            QStringLiteral("Winter"), QStringLiteral("Speech Contest"),
            QStringLiteral("Summer"), QStringLiteral("Fall")};
        for (int index = 0; index < columns.size(); ++index)
        {
            QSqlQuery insertColumn(store.database());
            insertColumn.prepare(QStringLiteral(
                "INSERT INTO roster_columns(class_id,name,position,width) "
                "VALUES (?,?,?,120)"));
            insertColumn.addBindValue(classId);
            insertColumn.addBindValue(columns.at(index));
            insertColumn.addBindValue(index);
            QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));
        }
        QSqlQuery insertCell(store.database());
        insertCell.prepare(QStringLiteral(
            "INSERT INTO roster_data(class_id,row_index,col_index,value) "
            "VALUES (?,0,0,?)"));
        insertCell.addBindValue(classId);
        insertCell.addBindValue(QStringLiteral("Saved"));
        QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));
        insertCell.prepare(QStringLiteral(
            "INSERT INTO roster_data(class_id,row_index,col_index,value) "
            "VALUES (?,0,1,?)"));
        insertCell.addBindValue(classId);
        insertCell.addBindValue(QStringLiteral("\uAE40\uBBFC\uC9C0"));
        QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));
    }

    PageHarness harness;
    harness.path = path;
    harness.saveMode = Application::SaveMode::Manual;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    QCOMPARE(page->selectedClassId()->value(), std::to_string(classId));
    page->sections()->setCurrentIndex(2);
    Ui::RosterEditorWidget* const roster = page->rosterEditor();
    QVERIFY(roster->isLoaded());
    QCOMPARE(roster->model()->data(roster->model()->index(0, 0)).toString(),
        QStringLiteral("Saved"));

    const auto setDraft = [roster](const QString& value) {
        return roster->model()->setData(roster->model()->index(0, 0), value);
    };
    QVERIFY(setDraft(QStringLiteral("Cancelled draft")));
    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Cancel;
    page->sections()->setCurrentIndex(0);
    QCOMPARE(page->sections()->currentIndex(), 2);
    QVERIFY(roster->isLoaded());
    QVERIFY(roster->hasUnsavedChanges());
    QCOMPARE(roster->model()->data(roster->model()->index(0, 0)).toString(),
        QStringLiteral("Cancelled Draft"));

    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Save;
    harness.failRosterSave = true;
    page->sections()->setCurrentIndex(0);
    QCOMPARE(page->sections()->currentIndex(), 2);
    QVERIFY(roster->isLoaded());
    QVERIFY(roster->hasUnsavedChanges());
    QCOMPARE(roster->model()->data(roster->model()->index(0, 0)).toString(),
        QStringLiteral("Cancelled Draft"));

    harness.failRosterSave = false;
    page->sections()->setCurrentIndex(0);
    QCOMPARE(page->sections()->currentIndex(), 0);
    QVERIFY(!roster->isLoaded());
    const auto afterSave = Platform::readRosterWindowAtPath(path, typedClassId(classId));
    QVERIFY(afterSave);
    QCOMPARE(afterSave.value().roster.rows[0][0], std::u16string(u"Cancelled Draft"));

    page->sections()->setCurrentIndex(2);
    QVERIFY(roster->isLoaded());
    QCOMPARE(roster->model()->data(roster->model()->index(0, 0)).toString(),
        QStringLiteral("Cancelled Draft"));
    QVERIFY(setDraft(QStringLiteral("Discarded draft")));
    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Discard;
    page->sections()->setCurrentIndex(0);
    QCOMPARE(page->sections()->currentIndex(), 0);
    QVERIFY(!roster->isLoaded());
    page->sections()->setCurrentIndex(2);
    QVERIFY(roster->isLoaded());
    QCOMPARE(roster->model()->data(roster->model()->index(0, 0)).toString(),
        QStringLiteral("Cancelled Draft"));
    QVERIFY(!roster->hasUnsavedChanges());
    const auto afterDiscard = Platform::readRosterWindowAtPath(path, typedClassId(classId));
    QVERIFY(afterDiscard);
    QCOMPARE(afterDiscard.value().roster.rows[0][0], std::u16string(u"Cancelled Draft"));
}

void TestingClassesManagerTests::
rosterDirtyChoicesSaveDiscardAndCancelBeforeClassSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-roster-dirty.tps"));
    int firstId = -1;
    int secondId = -1;
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
        firstId = seedTestingClass(store, QStringLiteral("First testing class"));
        secondId = seedTestingClass(store, QStringLiteral("Second testing class"));
        const QStringList columns{
            QStringLiteral("English"), QStringLiteral("Korean"),
            QStringLiteral("Winter"), QStringLiteral("Speech Contest"),
            QStringLiteral("Summer"), QStringLiteral("Fall")};
        const auto seedRoster = [&store, &columns](const int classId,
            const QString& english) {
            for (int index = 0; index < columns.size(); ++index)
            {
                QSqlQuery insertColumn(store.database());
                insertColumn.prepare(QStringLiteral(
                    "INSERT INTO roster_columns(class_id,name,position,width) "
                    "VALUES (?,?,?,120)"));
                insertColumn.addBindValue(classId);
                insertColumn.addBindValue(columns.at(index));
                insertColumn.addBindValue(index);
                if (!insertColumn.exec()) return false;
            }
            QSqlQuery insertNames(store.database());
            insertNames.prepare(QStringLiteral(
                "INSERT INTO roster_data(class_id,row_index,col_index,value) "
                "VALUES (?,?,?,?)"));
            insertNames.addBindValue(classId);
            insertNames.addBindValue(0);
            insertNames.addBindValue(0);
            insertNames.addBindValue(english);
            if (!insertNames.exec()) return false;
            insertNames.prepare(QStringLiteral(
                "INSERT INTO roster_data(class_id,row_index,col_index,value) "
                "VALUES (?,?,?,?)"));
            insertNames.addBindValue(classId);
            insertNames.addBindValue(0);
            insertNames.addBindValue(1);
            insertNames.addBindValue(QStringLiteral("\uAE40\uBBFC\uC9C0"));
            return insertNames.exec();
        };
        QVERIFY(firstId > 0);
        QVERIFY(secondId > firstId);
        QVERIFY(seedRoster(firstId, QStringLiteral("First saved")));
        QVERIFY(seedRoster(secondId, QStringLiteral("Second saved")));
    }

    PageHarness harness;
    harness.path = path;
    harness.saveMode = Application::SaveMode::Manual;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* const page = harness.page();
    QCOMPARE(page->selectedClassId()->value(), std::to_string(firstId));
    page->sections()->setCurrentIndex(2);
    QVERIFY(page->rosterEditor()->isLoaded());
    QCOMPARE(page->rosterEditor()->model()->data(
        page->rosterEditor()->model()->index(0, 0)).toString(),
        QStringLiteral("First saved"));

    QVERIFY(page->rosterEditor()->model()->setData(
        page->rosterEditor()->model()->index(0, 0), QStringLiteral("Cancelled draft")));
    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Cancel;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(secondId)));
    QCOMPARE(page->selectedClassId()->value(), std::to_string(firstId));
    QCOMPARE(page->rosterEditor()->model()->data(
        page->rosterEditor()->model()->index(0, 0)).toString(),
        QStringLiteral("Cancelled Draft"));
    QVERIFY(page->rosterEditor()->hasUnsavedChanges());

    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Discard;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(secondId)));
    QCOMPARE(page->selectedClassId()->value(), std::to_string(secondId));
    QVERIFY(!page->rosterEditor()->hasUnsavedChanges());
    const auto firstAfterDiscard = Platform::readRosterWindowAtPath(
        path, typedClassId(firstId));
    QVERIFY(firstAfterDiscard);
    QCOMPARE(firstAfterDiscard.value().roster.rows[0][0],
        std::u16string(u"First saved"));

    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(firstId)));
    QCOMPARE(page->rosterEditor()->model()->data(
        page->rosterEditor()->model()->index(0, 0)).toString(),
        QStringLiteral("First saved"));
    QVERIFY(page->rosterEditor()->model()->setData(
        page->rosterEditor()->model()->index(0, 0), QStringLiteral("Saved draft")));
    harness.unsavedChoice = Ui::TestingClassesUnsavedChoice::Save;
    page->classList()->setCurrentIndex(page->model()->indexForClassId(
        typedClassId(secondId)));
    QCOMPARE(page->selectedClassId()->value(), std::to_string(secondId));
    const auto firstAfterSave = Platform::readRosterWindowAtPath(
        path, typedClassId(firstId));
    QVERIFY(firstAfterSave);
    QCOMPARE(firstAfterSave.value().roster.rows[0][0],
        std::u16string(u"Saved Draft"));
    QVERIFY(!page->rosterEditor()->hasUnsavedChanges());
}

void TestingClassesManagerTests::ninetySixTestingClassesUseOneEditorTree()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("testing-large-list.tps"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(path));
    }
    PageHarness harness;
    harness.path = path;
    QVERIFY(harness.initialize());
    QVERIFY(harness.activate());
    auto* page = harness.page();
    QVERIFY(page);
    const int widgetCountBefore = widgetCount(page);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        for (int index = 0; index < 96; ++index)
        {
            const int id = seedTestingClass(store,
                QStringLiteral("Testing %1").arg(index + 1, 3, 10, QLatin1Char('0')));
            QVERIFY(id > 0);
        }
    }
    QVERIFY(page->refreshFromStore());
    QCOMPARE(page->model()->classCount(), 96);
    QCOMPARE(page->findChildren<QWidget*>(QStringLiteral("testingClassesEditor"))
        .size(), 1);
    QCOMPARE(widgetCount(page), widgetCountBefore);
}

QTEST_MAIN(TestingClassesManagerTests)
#include "testing_classes_manager_tests.moc"

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "domain/models/testing_class.h"
#include "next/application/schedule_slot_state_save.h"
#include "next/application/schedule_testing_assignment_save.h"
#include "next/platform/workspace_store_schedule_actions.h"
#include "next/platform/workspace_store_testing_classes_actions.h"
#include "sql_script_runner.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <optional>

using namespace ClassMngr::Next;

namespace
{

bool executeSql(WorkspaceStore& store, const QString& sql)
{
    return Test::executeSqlScript(store.database(), sql);
}

QString createWorkspace(QTemporaryDir& directory, const QString& name)
{
    const QString path = directory.filePath(name);
    WorkspaceStore store;
    if (!store.open(path))
    {
        return {};
    }
    QFile fixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/workspaces/large_startup.sql"));
    return fixture.open(QIODevice::ReadOnly)
        && executeSql(store, QString::fromUtf8(fixture.readAll()))
        ? path : QString{};
}

int createTestingClass(WorkspaceStore& store, const QString& name)
{
    TestingClassRepository* const repository = store.testingClassRepository();
    if (!repository)
    {
        return -1;
    }
    TestingClass testingClass;
    testingClass.name = name;
    testingClass.grade = QStringLiteral("M2");
    testingClass.level = QStringLiteral("Mixed (All)");
    testingClass.room = QStringLiteral("Room 9");
    const auto created = repository->createTestingClass(testingClass);
    return created ? created.value() : -1;
}

const Application::ScheduleTestingAssignmentReadRow* assignmentAt(
    const Application::SchedulePageReadSnapshot& snapshot,
    const QString& day,
    const QString& startTime)
{
    const auto found = std::find_if(
        snapshot.testingAssignments.rows.begin(),
        snapshot.testingAssignments.rows.end(),
        [&](const Application::ScheduleTestingAssignmentReadRow& row) {
            return QString::fromStdU16String(row.assignmentKey.day()) == day
                && QString::fromStdU16String(row.assignmentKey.startTime())
                    == startTime;
        });
    return found == snapshot.testingAssignments.rows.end()
        ? nullptr : &*found;
}

std::size_t countRows(const Application::ScheduleBuilderSourceSnapshot& source,
                      const bool intensive)
{
    std::size_t count = 0;
    for (const auto& row : source.classes)
    {
        count += intensive ? row.intensiveSchedule.size()
                           : row.regularSchedule.size();
    }
    return count;
}

bool setTeacherName(WorkspaceStore& store, const QString& value)
{
    QSqlQuery query(store.database());
    query.prepare(QStringLiteral(
        "UPDATE teachers SET teacher_en=? WHERE id=1"));
    query.addBindValue(value);
    return query.exec();
}

bool hasState(
    const Application::ScheduleSlotStateReadSnapshot& snapshot,
    const QString& day,
    const QString& startTime,
    const QString& state)
{
    for (const auto& row : snapshot.rows)
    {
        if (QString::fromStdU16String(row.identity.day()) == day
            && QString::fromStdU16String(row.identity.startTime()) == startTime
            && QString::fromStdU16String(row.state) == state)
        {
            return true;
        }
    }
    return false;
}

} // namespace

class ScheduleWorkspaceStoreActionsTests final : public QObject
{
    Q_OBJECT

private slots:
    void exactPathsIsolateIdenticalIdsPreferencesModesAndSlotKeys();
    void testingAssignmentsArePathScopedTransactionalAndRetryable();
    void failuresAndOversizedSourceAreReturnedAsErrors();
};

void ScheduleWorkspaceStoreActionsTests::exactPathsIsolateIdenticalIdsPreferencesModesAndSlotKeys()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString firstPath = createWorkspace(temporary, QStringLiteral("first.tps"));
    const QString secondPath = createWorkspace(temporary, QStringLiteral("second.tps"));
    QVERIFY(!firstPath.isEmpty());
    QVERIFY(!secondPath.isEmpty());

    WorkspaceStore firstStore;
    QVERIFY(firstStore.openExisting(firstPath));
    QVERIFY(setTeacherName(firstStore, QStringLiteral("First workspace teacher")));
    firstStore.close();

    WorkspaceStore secondStore;
    QVERIFY(secondStore.openExisting(secondPath));
    QVERIFY(setTeacherName(secondStore, QStringLiteral("Second workspace teacher")));
    secondStore.close();

    const auto firstBefore = Platform::readSchedulePageAtPath(firstPath);
    const auto secondBefore = Platform::readSchedulePageAtPath(secondPath);
    QVERIFY(firstBefore);
    QVERIFY(secondBefore);
    QCOMPARE(firstBefore.value().source.classes.size(), std::size_t(96));
    QCOMPARE(secondBefore.value().source.classes.size(), std::size_t(96));
    QCOMPARE(countRows(firstBefore.value().source, false), std::size_t(768));
    QCOMPARE(countRows(firstBefore.value().source, true), std::size_t(24));
    QCOMPARE(firstBefore.value().source.classes.front().classId.value(),
             std::string("1"));
    QCOMPARE(secondBefore.value().source.classes.front().classId.value(),
             std::string("1"));
    QCOMPARE(QString::fromStdU16String(
                 firstBefore.value().source.classes.front().teacherEnglishName),
             QStringLiteral("First workspace teacher"));
    QCOMPARE(QString::fromStdU16String(
                 secondBefore.value().source.classes.front().teacherEnglishName),
             QStringLiteral("Second workspace teacher"));

    const Application::ScheduleDisplayPreferences preferences{
        .use24HourTime = true,
        .showEnglishNames = true,
        .showWeekends = true,
        .showAllIntensiveHours = true,
        .testingAffectsM1 = false
    };
    QVERIFY(Platform::saveScheduleDisplayPreferencesAtPath(firstPath, preferences));
    QVERIFY(Platform::saveScheduleDisplayModeAtPath(
        firstPath, Application::ScheduleDisplayMode::Intensive));

    const Application::ScheduleSlotStateSaveRequest lunch{
        .weekday = Application::ScheduleWeekday::Monday,
        .startMinute = Application::ScheduleSlotStartMinute(20 * 60),
        .selectedState = Application::ScheduleSlotState::Lunch,
        .defaultState = Application::ScheduleSlotState::Essay
    };
    QVERIFY(Platform::saveScheduleSlotStateAtPath(firstPath, lunch));

    const auto firstAfterSave = Platform::readSchedulePageAtPath(firstPath);
    const auto secondUnchanged = Platform::readSchedulePageAtPath(secondPath);
    QVERIFY(firstAfterSave);
    QVERIFY(secondUnchanged);
    QCOMPARE(firstAfterSave.value().displayPreferences, preferences);
    QCOMPARE(firstAfterSave.value().displayMode,
             Application::ScheduleDisplayMode::Intensive);
    QVERIFY(hasState(firstAfterSave.value().slotStates,
                     QStringLiteral("Monday"), QStringLiteral("20:00"),
                     QStringLiteral("lunch")));
    QCOMPARE(secondUnchanged.value().displayPreferences,
             (Application::ScheduleDisplayPreferences{
                 .use24HourTime = false,
                 .showEnglishNames = true,
                 .showWeekends = true,
                 .showAllIntensiveHours = true,
                 .testingAffectsM1 = false
             }));
    QCOMPARE(secondUnchanged.value().displayMode,
             Application::ScheduleDisplayMode::Regular);
    QVERIFY(!hasState(secondUnchanged.value().slotStates,
                      QStringLiteral("Monday"), QStringLiteral("20:00"),
                      QStringLiteral("lunch")));

    const Application::ScheduleSlotStateSaveRequest defaultState{
        .weekday = Application::ScheduleWeekday::Monday,
        .startMinute = Application::ScheduleSlotStartMinute(20 * 60),
        .selectedState = Application::ScheduleSlotState::Essay,
        .defaultState = Application::ScheduleSlotState::Essay
    };
    QVERIFY(Platform::saveScheduleSlotStateAtPath(firstPath, defaultState));
    const auto afterDefault = Platform::readSchedulePageAtPath(firstPath);
    QVERIFY(afterDefault);
    QVERIFY(!hasState(afterDefault.value().slotStates,
                      QStringLiteral("Monday"), QStringLiteral("20:00"),
                      QStringLiteral("essay")));
    QVERIFY(!hasState(afterDefault.value().slotStates,
                      QStringLiteral("Monday"), QStringLiteral("20:00"),
                      QStringLiteral("lunch")));
}

void ScheduleWorkspaceStoreActionsTests::
testingAssignmentsArePathScopedTransactionalAndRetryable()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString firstPath = createWorkspace(
        temporary, QStringLiteral("assignments-first.tps"));
    const QString secondPath = createWorkspace(
        temporary, QStringLiteral("assignments-second.tps"));
    QVERIFY(!firstPath.isEmpty());
    QVERIFY(!secondPath.isEmpty());

    int firstClassId = -1;
    int replacementClassId = -1;
    int overlappingClassId = -1;
    {
        WorkspaceStore firstStore;
        WorkspaceStore secondStore;
        QVERIFY(firstStore.openExisting(firstPath));
        QVERIFY(secondStore.openExisting(secondPath));
        firstClassId = createTestingClass(
            firstStore, QStringLiteral("First testing class"));
        replacementClassId = createTestingClass(
            firstStore, QStringLiteral("Replacement testing class"));
        overlappingClassId = createTestingClass(
            secondStore, QStringLiteral("Second workspace class"));
        QVERIFY(firstClassId > 0);
        QVERIFY(replacementClassId > firstClassId);
        QCOMPARE(overlappingClassId, firstClassId);
        QVERIFY(firstStore.settingsRepository()->saveSetting(
            QStringLiteral("myInfo/name"), QStringLiteral("First Profile")));
        QVERIFY(secondStore.settingsRepository()->saveSetting(
            QStringLiteral("myInfo/name"), QStringLiteral("Second Profile")));
    }

    using Mutation = Application::ScheduleTestingAssignmentMutation;
    const auto firstChoices = Platform::readTestingClassesAtPath(firstPath);
    const auto secondChoices = Platform::readTestingClassesAtPath(secondPath);
    QVERIFY(firstChoices);
    QVERIFY(secondChoices);
    const auto choiceById = [](const auto& choices, const int id) {
        return std::find_if(choices.choices.begin(), choices.choices.end(),
            [id](const auto& choice) {
                return choice.classId.value() == std::to_string(id);
            });
    };
    const auto firstChoice = choiceById(firstChoices.value(), firstClassId);
    const auto secondChoice = choiceById(secondChoices.value(), overlappingClassId);
    QVERIFY(firstChoice != firstChoices.value().choices.end());
    QVERIFY(secondChoice != secondChoices.value().choices.end());
    QCOMPARE(QString::fromStdU16String(firstChoice->name),
             QStringLiteral("First testing class"));
    QCOMPARE(QString::fromStdU16String(secondChoice->name),
             QStringLiteral("Second workspace class"));

    const auto firstPlain = Application::ScheduleTestingAssignmentSaveRequest{
        .mutation = Mutation::SavePlainTesting,
        .day = u"Monday",
        .startTime = u"16:00"
    };
    QVERIFY(Platform::saveScheduleTestingAssignmentAtPath(firstPath, firstPlain));
    auto secondPlain = firstPlain;
    secondPlain.room = u"Room B";
    QVERIFY(Platform::saveScheduleTestingAssignmentAtPath(secondPath, secondPlain));

    auto firstRead = Platform::readSchedulePageAtPath(firstPath);
    auto secondRead = Platform::readSchedulePageAtPath(secondPath);
    QVERIFY(firstRead);
    QVERIFY(secondRead);
    const auto* firstAssignment = assignmentAt(
        firstRead.value(), QStringLiteral("Monday"), QStringLiteral("16:00"));
    const auto* secondAssignment = assignmentAt(
        secondRead.value(), QStringLiteral("Monday"), QStringLiteral("16:00"));
    QVERIFY(firstAssignment);
    QVERIFY(secondAssignment);
    QCOMPARE(QString::fromStdU16String(firstAssignment->room), QString{});
    QCOMPARE(QString::fromStdU16String(secondAssignment->room),
             QStringLiteral("Room B"));

    auto editedPlain = firstPlain;
    editedPlain.room = u"Room A";
    editedPlain.replaceExisting = true;
    QVERIFY(Platform::saveScheduleTestingAssignmentAtPath(firstPath, editedPlain));
    auto classAssignment = Application::ScheduleTestingAssignmentSaveRequest{
        .mutation = Mutation::AssignTestingClass,
        .day = u"Monday",
        .startTime = u"16:00",
        .classId = Domain::ClassId::fromString(std::to_string(firstClassId)),
        .replaceExisting = true
    };
    QVERIFY(classAssignment.classId);
    QVERIFY(Platform::saveScheduleTestingAssignmentAtPath(firstPath, classAssignment));
    classAssignment.classId = Domain::ClassId::fromString(
        std::to_string(replacementClassId));
    QVERIFY(Platform::saveScheduleTestingAssignmentAtPath(firstPath, classAssignment));

    firstRead = Platform::readSchedulePageAtPath(firstPath);
    secondRead = Platform::readSchedulePageAtPath(secondPath);
    QVERIFY(firstRead);
    QVERIFY(secondRead);
    firstAssignment = assignmentAt(
        firstRead.value(), QStringLiteral("Monday"), QStringLiteral("16:00"));
    secondAssignment = assignmentAt(
        secondRead.value(), QStringLiteral("Monday"), QStringLiteral("16:00"));
    QVERIFY(firstAssignment);
    QVERIFY(secondAssignment);
    QCOMPARE(firstAssignment->classId.value(), std::to_string(replacementClassId));
    QVERIFY(firstAssignment->specialClass);
    QCOMPARE(QString::fromStdU16String(firstAssignment->specialClass->name),
             QStringLiteral("Replacement testing class"));
    QCOMPARE(QString::fromStdU16String(secondAssignment->room),
             QStringLiteral("Room B"));
    QCOMPARE(secondAssignment->classId.value(), std::string("-1"));

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(firstPath));
        QSqlQuery failReplacement(store.database());
        QVERIFY2(failReplacement.exec(QStringLiteral(R"(
            CREATE TRIGGER fail_testing_assignment_replacement
            BEFORE UPDATE ON schedule_testing_blocks
            WHEN NEW.day = 'Monday' AND NEW.start_time = '16:00'
            BEGIN SELECT RAISE(ABORT, 'forced assignment transaction failure'); END
        )")), qPrintable(failReplacement.lastError().text()));
    }
    classAssignment.classId = Domain::ClassId::fromString(
        std::to_string(firstClassId));
    const auto failedReplacement =
        Platform::saveScheduleTestingAssignmentAtPath(firstPath, classAssignment);
    QVERIFY(!failedReplacement);
    firstRead = Platform::readSchedulePageAtPath(firstPath);
    QVERIFY(firstRead);
    firstAssignment = assignmentAt(
        firstRead.value(), QStringLiteral("Monday"), QStringLiteral("16:00"));
    QVERIFY(firstAssignment);
    QCOMPARE(firstAssignment->classId.value(), std::to_string(replacementClassId));

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(firstPath));
        QSqlQuery removeTrigger(store.database());
        QVERIFY2(removeTrigger.exec(QStringLiteral(
            "DROP TRIGGER fail_testing_assignment_replacement")),
            qPrintable(removeTrigger.lastError().text()));
    }
    QVERIFY(Platform::saveScheduleTestingAssignmentAtPath(firstPath, classAssignment));

    const Application::ScheduleTestingAssignmentSaveRequest removeAssignment{
        .mutation = Mutation::RemoveAssignment,
        .day = u"Monday",
        .startTime = u"16:00"
    };
    QVERIFY(Platform::saveScheduleTestingAssignmentAtPath(
        firstPath, removeAssignment));
    firstRead = Platform::readSchedulePageAtPath(firstPath);
    QVERIFY(firstRead);
    QVERIFY(!assignmentAt(firstRead.value(), QStringLiteral("Monday"),
                          QStringLiteral("16:00")));

    const Application::ScheduleTestingAssignmentSaveRequest retryAssignment{
        .mutation = Mutation::AssignTestingClass,
        .day = u"Tuesday",
        .startTime = u"17:00",
        .classId = Domain::ClassId::fromString(std::to_string(firstClassId))
    };
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(firstPath));
        QSqlQuery failInsert(store.database());
        QVERIFY2(failInsert.exec(QStringLiteral(R"(
            CREATE TRIGGER fail_testing_assignment_insert
            BEFORE INSERT ON schedule_testing_blocks
            WHEN NEW.day = 'Tuesday' AND NEW.start_time = '17:00'
            BEGIN SELECT RAISE(ABORT, 'forced assignment insert failure'); END
        )")), qPrintable(failInsert.lastError().text()));
    }
    QVERIFY(!Platform::saveScheduleTestingAssignmentAtPath(
        firstPath, retryAssignment));
    firstRead = Platform::readSchedulePageAtPath(firstPath);
    QVERIFY(firstRead);
    QVERIFY(!assignmentAt(firstRead.value(), QStringLiteral("Tuesday"),
                          QStringLiteral("17:00")));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(firstPath));
        QSqlQuery removeTrigger(store.database());
        QVERIFY2(removeTrigger.exec(QStringLiteral(
            "DROP TRIGGER fail_testing_assignment_insert")),
            qPrintable(removeTrigger.lastError().text()));
    }
    QVERIFY(Platform::saveScheduleTestingAssignmentAtPath(
        firstPath, retryAssignment));
    firstRead = Platform::readSchedulePageAtPath(firstPath);
    QVERIFY(firstRead);
    const auto* retried = assignmentAt(
        firstRead.value(), QStringLiteral("Tuesday"), QStringLiteral("17:00"));
    QVERIFY(retried);
    QCOMPARE(retried->classId.value(), std::to_string(firstClassId));

    const auto firstProfile = Platform::readScheduleOutputProfileNameAtPath(firstPath);
    const auto secondProfile = Platform::readScheduleOutputProfileNameAtPath(secondPath);
    QVERIFY(firstProfile);
    QVERIFY(secondProfile);
    QCOMPARE(QString::fromStdString(firstProfile.value()),
             QStringLiteral("First Profile"));
    QCOMPARE(QString::fromStdString(secondProfile.value()),
             QStringLiteral("Second Profile"));
    QVERIFY(!Platform::saveScheduleTestingAssignmentAtPath(
        temporary.filePath(QStringLiteral("missing.tps")), firstPlain));
    QVERIFY(!Platform::readScheduleOutputProfileNameAtPath(
        temporary.filePath(QStringLiteral("missing.tps"))));
}

void ScheduleWorkspaceStoreActionsTests::failuresAndOversizedSourceAreReturnedAsErrors()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createWorkspace(temporary, QStringLiteral("workspace.tps"));
    QVERIFY(!path.isEmpty());

    const auto displayBeforeFailures = Platform::readSchedulePageAtPath(path);
    QVERIFY(displayBeforeFailures);

    WorkspaceStore settingsFailureStore;
    QVERIFY(settingsFailureStore.openExisting(path));
    QSqlQuery failModeSave(settingsFailureStore.database());
    QVERIFY2(failModeSave.exec(QStringLiteral(R"(
        CREATE TRIGGER fail_schedule_mode_save
        BEFORE INSERT ON app_settings
        WHEN NEW.key = 'schedule_display_mode'
        BEGIN
            SELECT RAISE(ABORT, 'forced schedule mode save failure');
        END
    )")), qPrintable(failModeSave.lastError().text()));
    QSqlQuery failPreferencesSave(settingsFailureStore.database());
    QVERIFY2(failPreferencesSave.exec(QStringLiteral(R"(
        CREATE TRIGGER fail_schedule_preferences_save
        BEFORE INSERT ON app_settings
        WHEN NEW.key = 'schedule_use_24h'
        BEGIN
            SELECT RAISE(ABORT, 'forced schedule preferences save failure');
        END
    )")), qPrintable(failPreferencesSave.lastError().text()));
    settingsFailureStore.close();

    const auto failedModeSave = Platform::saveScheduleDisplayModeAtPath(
        path, Application::ScheduleDisplayMode::Intensive);
    QVERIFY(!failedModeSave);
    QVERIFY(std::string(failedModeSave.error().message).find(
                "forced schedule mode save failure") != std::string::npos);
    const auto failedPreferencesSave =
        Platform::saveScheduleDisplayPreferencesAtPath(path, {
            .use24HourTime = !displayBeforeFailures.value().displayPreferences.use24HourTime,
            .showEnglishNames = !displayBeforeFailures.value().displayPreferences.showEnglishNames,
            .showWeekends = !displayBeforeFailures.value().displayPreferences.showWeekends,
            .showAllIntensiveHours = !displayBeforeFailures.value().displayPreferences.showAllIntensiveHours,
            .testingAffectsM1 = !displayBeforeFailures.value().displayPreferences.testingAffectsM1
        });
    QVERIFY(!failedPreferencesSave);
    QVERIFY(std::string(failedPreferencesSave.error().message).find(
                "forced schedule preferences save failure") != std::string::npos);
    const auto displayAfterFailures = Platform::readSchedulePageAtPath(path);
    QVERIFY(displayAfterFailures);
    QCOMPARE(displayAfterFailures.value().displayMode,
             displayBeforeFailures.value().displayMode);
    QCOMPARE(displayAfterFailures.value().displayPreferences,
             displayBeforeFailures.value().displayPreferences);

    const auto missing = Platform::readSchedulePageAtPath(
        temporary.filePath(QStringLiteral("missing.tps")));
    QVERIFY(!missing);
    const auto failedSave = Platform::saveScheduleSlotStateAtPath(
        temporary.filePath(QStringLiteral("missing.tps")), {
            .weekday = Application::ScheduleWeekday::Monday,
            .startMinute = Application::ScheduleSlotStartMinute(12 * 60),
            .selectedState = Application::ScheduleSlotState::Lunch,
            .defaultState = Application::ScheduleSlotState::Essay
        });
    QVERIFY(!failedSave);

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QSqlQuery removeIntensiveSource(store.database());
    QVERIFY2(removeIntensiveSource.exec(QStringLiteral(
        "DROP TABLE class_intensive_times")),
        qPrintable(removeIntensiveSource.lastError().text()));
    store.close();
    const auto brokenSource = Platform::readSchedulePageAtPath(path);
    QVERIFY(!brokenSource);
    QVERIFY(!brokenSource.error().message.empty());

    const QString oversizedPath = createWorkspace(
        temporary, QStringLiteral("oversized.tps"));
    QVERIFY(!oversizedPath.isEmpty());
    WorkspaceStore oversizedStore;
    QVERIFY(oversizedStore.openExisting(oversizedPath));
    QSqlQuery extraClasses(oversizedStore.database());
    QVERIFY2(extraClasses.exec(QStringLiteral(R"(
        WITH RECURSIVE extra(id) AS (
            SELECT 1000
            UNION ALL SELECT id + 1 FROM extra WHERE id < 5000
        )
        INSERT INTO classes (id, name)
        SELECT id, printf('Bounded class %d', id) FROM extra
    )")), qPrintable(extraClasses.lastError().text()));
    QSqlQuery extraInformation(oversizedStore.database());
    QVERIFY2(extraInformation.exec(QStringLiteral(R"(
        WITH RECURSIVE extra(id) AS (
            SELECT 1000
            UNION ALL SELECT id + 1 FROM extra WHERE id < 5000
        )
        INSERT INTO class_info (class_id, teacher_id, class_grade, class_level)
        SELECT id, 1, 'E4', 'Theseus' FROM extra
    )")), qPrintable(extraInformation.lastError().text()));
    oversizedStore.close();
    const auto overLimit = Platform::readSchedulePageAtPath(oversizedPath);
    QVERIFY(!overLimit);
    QVERIFY(QString::fromUtf8(
                overLimit.error().message.data(),
                static_cast<qsizetype>(overLimit.error().message.size()))
                .contains(QStringLiteral("4,096")));
}

QTEST_GUILESS_MAIN(ScheduleWorkspaceStoreActionsTests)
#include "schedule_workspace_store_actions_tests.moc"

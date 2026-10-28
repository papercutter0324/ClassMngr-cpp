#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/settings_repository.h"
#include "next/application/schedule_slot_state_save.h"
#include "next/platform/workspace_store_schedule_actions.h"
#include "sql_script_runner.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

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

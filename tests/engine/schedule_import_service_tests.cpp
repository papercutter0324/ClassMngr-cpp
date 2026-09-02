#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/schedule_import_rules.h"
#include "classmngr/engine/schedule_import_service.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/teacher_service.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using namespace classmngr::engine;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineScheduleImportServiceTests: "
              << message
              << '\n';
    return false;
}

int countRows(
    SqliteDatabase& database,
    std::string_view sql
    )
{
    const auto result = database.query(sql);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return -1;
    }
    const auto* value = std::get_if<std::int64_t>(
        &result->rows.front().values.front()
        );
    return value == nullptr ? -1 : static_cast<int>(*value);
}

ClassInfo info(
    int classId,
    int teacherId,
    std::vector<ClassTime> regularTimes = {},
    std::vector<ClassTime> intensiveTimes = {}
    )
{
    ClassInfo result;
    result.classId = classId;
    result.teacherId = teacherId;
    result.classGrade = "E5";
    result.classLevel = "Zeus";
    result.readingBook = "Reading Explorer 5";
    result.essayBook = "5E";
    result.classColor = "#ABCDEF";
    result.fontColor = "#123456";
    result.classTimes = std::move(regularTimes);
    result.intensiveTimes = std::move(intensiveTimes);
    return result;
}

Teacher koreanTeacher(std::string room)
{
    Teacher result;
    result.teacherKr = "홍길동";
    result.roomNumber = std::move(room);
    return result;
}

ScheduleImportClassCandidate candidate(
    std::string teacherKey = "홍길동",
    std::string level = "Zeus",
    std::vector<ClassTime> times = {
        {"Monday", "4:00 PM", "4:55 PM"},
        {"Wednesday", "4:00 PM", "4:55 PM"}
    }
    )
{
    ScheduleImportClassCandidate result;
    result.teacherKey = std::move(teacherKey);
    result.teacherKr = result.teacherKey;
    result.rooms = {"413"};
    result.classGrade = "E5";
    result.classLevel = std::move(level);
    result.times = std::move(times);
    return result;
}

ScheduleImportPlan planFor(
    ScheduleImportKind kind,
    const ScheduleImportClassCandidate& imported,
    ScheduleImportClassAction classAction,
    int targetClassId,
    ScheduleImportTeacherAction teacherAction,
    int targetTeacherId,
    std::string selectedRoom = "413"
    )
{
    ScheduleImportPlan result;
    result.kind = kind;
    result.unknownCellsAcknowledged = true;
    result.candidates = {imported};
    result.teachers = {
        {
            imported.teacherKey,
            teacherAction,
            targetTeacherId,
            std::move(selectedRoom)
        }
    };
    result.classes = {
        {
            0,
            classAction,
            targetClassId,
            "#123456",
            "#FFFFFF"
        }
    };
    return result;
}

bool sameTimes(
    const std::vector<ClassTime>& left,
    const std::vector<ClassTime>& right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (left[index].day != right[index].day
            || left[index].startTime != right[index].startTime
            || left[index].endTime != right[index].endTime)
        {
            return false;
        }
    }
    return true;
}

bool scheduleImportRulesMatchServicePolicy()
{
    ClassInfo existing;
    existing.classGrade = "E5";
    existing.classLevel = "Zeus";
    existing.classTimes = {
        {"Monday", "4:00 PM", "4:55 PM"},
        {"Wednesday", "4:00 PM", "4:55 PM"}
    };
    existing.intensiveTimes = {
        {"Tuesday", "9:00 AM", "9:50 AM"}
    };

    const ScheduleImportClassCandidate imported = candidate();
    const ScheduleImportClassCandidate invalidPattern = candidate(
        "홍길동",
        "Zeus",
        {
            {"Monday", "4:00 PM", "4:55 PM"},
            {"Tuesday", "4:00 PM", "4:55 PM"}
        }
        );
    const ScheduleImportClassCandidate duplicatePattern = candidate(
        "홍길동",
        "Zeus",
        {
            {"Monday", "4:00 PM", "4:55 PM"},
            {"Monday", "5:00 PM", "5:55 PM"}
        }
        );
    const std::vector<ClassTime> mixedGroups{
        {"Monday", "4:00 PM", "4:55 PM"},
        {"Tuesday", "4:00 PM", "4:55 PM"}
    };

    const auto validPattern = ScheduleImportRules::validateMeetingPattern(
        imported
        );
    const auto invalid = ScheduleImportRules::validateMeetingPattern(
        invalidPattern
        );
    const auto duplicate = ScheduleImportRules::validateMeetingPattern(
        duplicatePattern
        );
    const auto allowed = ScheduleImportRules::allowedDayPatterns(
        "E5",
        "Zeus"
        );
    const bool passed = expect(
        ScheduleImportRules::weekdayIndex(" wEdNeSdAy ") == 2
            && ScheduleImportRules::dayGroup(imported.times) == 1
            && ScheduleImportRules::dayGroup(mixedGroups) == 0
            && ScheduleImportRules::daysAreCompatible(
                imported.times,
                existing.classTimes
                )
            && ScheduleImportRules::meetingDaysMatch(
                imported.times,
                existing.classTimes
                )
            && sameTimes(
                ScheduleImportRules::timesForKind(
                    existing,
                    ScheduleImportKind::Intensive
                    ),
                existing.intensiveTimes
                )
            && sameTimes(
                ScheduleImportRules::timesForKind(
                    ClassInfo{.classTimes = existing.classTimes},
                    ScheduleImportKind::Intensive
                    ),
                existing.classTimes
                )
            && sameTimes(
                ScheduleImportRules::targetTimesForKind(
                    existing,
                    ScheduleImportKind::Normal
                    ),
                existing.classTimes
                )
            && ScheduleImportRules::classOptionIsEligible(
                imported,
                existing,
                ScheduleImportKind::Normal
                )
            && allowed.size() == 4
            && validPattern.status
                == ScheduleImportMeetingPatternStatus::Valid
            && validPattern.meetingDays
                == std::vector<std::string>{"Monday", "Wednesday"}
            && invalid.status
                == ScheduleImportMeetingPatternStatus::UnsupportedPattern
            && duplicate.status
                == ScheduleImportMeetingPatternStatus::InvalidWeekdayOrDuplicate
            && ScheduleImportRules::meetingPatternExpectation(
                "E5",
                "Athena"
                )
                == ScheduleImportMeetingPatternExpectation::
                    WeekdayTripleOrTuesdayThursday
            && ScheduleImportRules::meetingPatternExpectation(
                "E6",
                "Hera"
                ) == ScheduleImportMeetingPatternExpectation::OneWeekday,
        "schedule-import rules contract changed its matching or pattern policy"
        );
    return passed;
}

bool validatesWithoutWriting()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    SqliteDatabase& database = **opened;
    ScheduleImportService imports(database);
    const ScheduleImportPlan plan = planFor(
        ScheduleImportKind::Normal,
        candidate("김하늘", "Apollo"),
        ScheduleImportClassAction::CreateNew,
        -1,
        ScheduleImportTeacherAction::Create,
        -1
        );

    const int teachersBefore = countRows(
        database,
        "SELECT COUNT(*) FROM teachers"
        );
    const int classesBefore = countRows(
        database,
        "SELECT COUNT(*) FROM classes"
        );
    const int timesBefore = countRows(
        database,
        "SELECT COUNT(*) FROM class_times"
        );
    const Status validated = imports.validateImport(plan);
    const bool passed = expect(
        validated
            && countRows(database, "SELECT COUNT(*) FROM teachers")
                == teachersBefore
            && countRows(database, "SELECT COUNT(*) FROM classes")
                == classesBefore
            && countRows(database, "SELECT COUNT(*) FROM class_times")
                == timesBefore,
        "valid schedule import validation did not remain read-only"
        );

    database.close();
    return passed;
}

bool rejectsProjectedConflict()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    SqliteDatabase& database = **opened;
    TeacherService teachers(database);
    ClassRepository classes(database);
    ClassInfoService classInfo(database);
    ScheduleImportService imports(database);
    bool passed = true;

    const auto teacherId = teachers.create(koreanTeacher("413"));
    const auto targetClassId = classes.create("Target");
    const auto absentClassId = classes.create("Absent");
    passed &= expect(
        teacherId && targetClassId && absentClassId,
        "projected conflict fixtures could not be created"
        );
    if (!teacherId || !targetClassId || !absentClassId)
    {
        return false;
    }

    passed &= expect(
        classInfo.save(info(
            *targetClassId,
            *teacherId,
            {},
            {{"Monday", "9:00 AM", "9:50 AM"}}
            )).has_value()
            && classInfo.save(info(
                *absentClassId,
                *teacherId,
                {},
                {{"Monday", "10:00 AM", "10:50 AM"}}
                )).has_value(),
        "projected conflict fixture information could not be saved"
        );

    const ScheduleImportPlan plan = planFor(
        ScheduleImportKind::Intensive,
        candidate(
            "홍길동",
            "Zeus",
            {
                {"Monday", "10:00 AM", "10:50 AM"},
                {"Wednesday", "10:00 AM", "10:50 AM"}
            }
            ),
        ScheduleImportClassAction::UpdateExisting,
        *targetClassId,
        ScheduleImportTeacherAction::Reuse,
        *teacherId
        );
    const int classesBefore = countRows(
        database,
        "SELECT COUNT(*) FROM classes"
        );
    const int timesBefore = countRows(
        database,
        "SELECT COUNT(*) FROM class_intensive_times"
        );
    const Status validated = imports.validateImport(plan);
    passed &= expect(
        !validated
            && validated.error().message.find("proposed schedule overlaps")
                != std::string::npos
            && countRows(database, "SELECT COUNT(*) FROM classes")
                == classesBefore
            && countRows(database, "SELECT COUNT(*) FROM class_intensive_times")
                == timesBefore,
        "projected schedule conflict was not rejected read-only"
        );

    database.close();
    return passed;
}

bool previewMatchesAndRanks()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    SqliteDatabase& database = **opened;
    TeacherService teachers(database);
    ClassRepository classes(database);
    ClassInfoService classInfo(database);
    ScheduleImportService imports(database);
    bool passed = true;

    const auto teacherId = teachers.create(koreanTeacher("413"));
    const auto exactClassId = classes.create("Existing Zeus");
    const auto possibleClassId = classes.create("Possible Zeus");
    passed &= expect(
        teacherId && exactClassId && possibleClassId,
        "preview fixtures could not be created"
        );
    if (!teacherId || !exactClassId || !possibleClassId)
    {
        return false;
    }

    passed &= expect(
        classInfo.save(info(
            *exactClassId,
            *teacherId,
            {
                {"Monday", "4:00 PM", "4:55 PM"},
                {"Wednesday", "4:00 PM", "4:55 PM"}
            }
            )).has_value(),
        "exact preview class information could not be saved"
        );
    passed &= expect(
        classInfo.save(info(*possibleClassId, *teacherId)).has_value(),
        "possible preview class information could not be saved"
        );

    ScheduleImportUserBlock user;
    user.name = "Alice";
    user.classes = {candidate()};
    const auto preview = imports.previewImport(user, ScheduleImportKind::Normal);
    passed &= expect(
        preview
            && preview->inventory.classCount == 2
            && preview->inventory.hasRegularHours
            && !preview->inventory.hasIntensiveHours
            && preview->teachers.size() == 1
            && preview->teachers.front().matchingTeacherIds
                == std::vector<int>{*teacherId}
            && preview->teachers.front().affectedClassCount == 2
            && preview->classes.size() == 1
            && preview->classes.front().exactMatch
            && preview->classes.front().suggestedClassId == *exactClassId
            && preview->classes.front().matchingClassIds.size() == 2
            && preview->initiallyAbsentClassIds.size() == 1
            && preview->initiallyAbsentClassIds.front() == *possibleClassId,
        "preview did not preserve the ranked match and inventory contract"
        );

    database.close();
    return passed;
}

bool createsSnapshotAndRollsBack()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    SqliteDatabase& database = **opened;
    ClassRepository classes(database);
    ClassInfoService classInfo(database);
    ScheduleImportService imports(database);
    bool passed = true;

    const auto absentClassId = classes.create("Absent");
    passed &= expect(absentClassId.has_value(), "snapshot fixture class failed");
    if (!absentClassId)
    {
        return false;
    }
    passed &= expect(
        classInfo.save(info(
            *absentClassId,
            -1,
            {{"Friday", "6:00 PM", "6:55 PM"}}
            )).has_value(),
        "snapshot fixture information failed"
        );

    ScheduleImportClassCandidate imported = candidate(
        "김하늘",
        "Apollo"
        );
    ScheduleImportPlan plan = planFor(
        ScheduleImportKind::Normal,
        imported,
        ScheduleImportClassAction::CreateNew,
        -1,
        ScheduleImportTeacherAction::Create,
        -1
        );
    plan.selectedUserName = "Alice";
    plan.saveProfileNameIfBlank = true;
    plan.diagnostics.push_back({"Current", "Alice", "C2", "Meeting", "ignored"});

    const auto result = imports.importSchedule(plan);
    const std::string resultError = result
        ? std::string{}
        : result.error().message;
    passed &= expect(
        result
            && result->teachersCreated == 1
            && result->classesCreated == 1
            && result->schedulesCleared == 1
            && result->ignoredCells == 1
            && result->profileNameUpdated
            && countRows(database, "SELECT COUNT(*) FROM teachers") == 1
            && countRows(database, "SELECT COUNT(*) FROM classes") == 2
            && countRows(database, "SELECT COUNT(*) FROM class_times") == 2
            && countRows(
                database,
                "SELECT COUNT(*) FROM app_settings WHERE key='myInfo/name'"
                ) == 1,
        "normal schedule import did not create a complete snapshot"
            + (result ? std::string{} : ": " + resultError)
        );

    const Status trigger = database.execute(
        "CREATE TRIGGER reject_imported_time "
        "BEFORE INSERT ON class_times BEGIN "
        "SELECT RAISE(ABORT, 'forced schedule failure'); END"
        );
    passed &= expect(trigger.has_value(), "rollback trigger could not be created");

    ScheduleImportClassCandidate rollbackCandidate = candidate(
        "박바다",
        "Zeus"
        );
    const int teachersBefore = countRows(
        database,
        "SELECT COUNT(*) FROM teachers"
        );
    const int classesBefore = countRows(
        database,
        "SELECT COUNT(*) FROM classes"
        );
    const auto failed = imports.importSchedule(planFor(
        ScheduleImportKind::Normal,
        rollbackCandidate,
        ScheduleImportClassAction::CreateNew,
        -1,
        ScheduleImportTeacherAction::Create,
        -1
        ));
    passed &= expect(
        !failed
            && failed.error().message.find("forced schedule failure")
                != std::string::npos
            && countRows(database, "SELECT COUNT(*) FROM teachers")
                == teachersBefore
            && countRows(database, "SELECT COUNT(*) FROM classes")
                == classesBefore,
        "schedule import did not roll back all writes"
        );

    database.close();
    return passed;
}

bool preservesAndReplacesIntensiveSnapshot()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    SqliteDatabase& database = **opened;
    TeacherService teachers(database);
    ClassRepository classes(database);
    ClassInfoService classInfo(database);
    ScheduleImportService imports(database);
    bool passed = true;

    const auto teacherId = teachers.create(koreanTeacher("413"));
    const auto importedClassId = classes.create("Imported");
    const auto absentClassId = classes.create("Absent");
    passed &= expect(
        teacherId && importedClassId && absentClassId,
        "intensive fixtures could not be created"
        );
    if (!teacherId || !importedClassId || !absentClassId)
    {
        return false;
    }

    passed &= expect(
        classInfo.save(info(
            *importedClassId,
            *teacherId,
            {},
            {{"Monday", "9:00 AM", "9:50 AM"}}
            )).has_value()
            && classInfo.save(info(
                *absentClassId,
                *teacherId,
                {},
                {{"Tuesday", "9:00 AM", "9:50 AM"}}
                )).has_value(),
        "intensive fixture information could not be saved"
        );

    ScheduleImportClassCandidate imported = candidate(
        "홍길동",
        "Zeus",
        {
            {"Monday", "10:00 AM", "10:50 AM"},
            {"Wednesday", "10:00 AM", "10:50 AM"}
        }
        );
    ScheduleImportPlan plan = planFor(
        ScheduleImportKind::Intensive,
        imported,
        ScheduleImportClassAction::UpdateExisting,
        *importedClassId,
        ScheduleImportTeacherAction::Reuse,
        *teacherId
        );
    plan.intensiveSlotStates = {
        {"Monday", "09:00", "empty"},
        {"Tuesday", "09:00", "lunch"}
    };

    const auto preserved = imports.importSchedule(plan);
    passed &= expect(
        preserved
            && preserved->classesUpdated == 1
            && preserved->schedulesCleared == 0
            && countRows(
                database,
                "SELECT COUNT(*) FROM class_intensive_times"
                ) == 3
            && countRows(
                database,
                "SELECT COUNT(*) FROM intensive_slot_states"
                ) == 2,
        "intensive update mode did not preserve absent classes"
        );

    plan.intensiveMode = ScheduleImportIntensiveMode::ReplaceWithNew;
    const auto replaced = imports.importSchedule(plan);
    passed &= expect(
        replaced
            && replaced->schedulesCleared == 1
            && countRows(
                database,
                "SELECT COUNT(*) FROM class_intensive_times"
                ) == 2
            && countRows(
                database,
                "SELECT COUNT(*) FROM class_intensive_times "
                "WHERE class_id=" + std::to_string(*absentClassId)
                ) == 0,
        "intensive replace mode did not clear absent classes"
        );

    database.close();
    return passed;
}
} // namespace

int main()
{
    const bool passed = scheduleImportRulesMatchServicePolicy()
        && validatesWithoutWriting()
        && rejectsProjectedConflict()
        && previewMatchesAndRanks()
        && createsSnapshotAndRollsBack()
        && preservesAndReplacesIntensiveSnapshot();
    return passed ? 0 : 1;
}

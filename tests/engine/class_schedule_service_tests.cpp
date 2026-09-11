#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_schedule_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/teacher_service.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using classmngr::engine::ClassConflict;
using classmngr::engine::ClassInfo;
using classmngr::engine::ClassInfoService;
using classmngr::engine::ClassRepository;
using classmngr::engine::ClassScheduleService;
using classmngr::engine::ClassTime;
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::SqliteParameters;
using classmngr::engine::SqliteValue;
using classmngr::engine::Teacher;
using classmngr::engine::TeacherService;
using classmngr::engine::ScheduleType;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineClassScheduleServiceTests: "
              << message
              << '\n';
    return false;
}

Teacher teacher(std::string name)
{
    Teacher result;
    result.teacherEn = std::move(name);
    return result;
}

ClassInfo info(
    int classId,
    int teacherId,
    std::vector<ClassTime> regular,
    std::vector<ClassTime> intensive = {}
    )
{
    ClassInfo result;
    result.classId = classId;
    result.teacherId = teacherId;
    result.classGrade = "E4";
    result.classLevel = "Theseus";
    result.readingBook = "";
    result.essayBook = "";
    result.classColor = "#ABCDEF";
    result.fontColor = "#123456";
    result.classTimes = std::move(regular);
    result.intensiveTimes = std::move(intensive);
    return result;
}

const ClassInfo* findInfo(
    const std::vector<ClassInfo>& infos,
    int classId
    )
{
    for (const ClassInfo& info : infos)
    {
        if (info.classId == classId)
        {
            return &info;
        }
    }
    return nullptr;
}

int countConflictsFor(
    const std::vector<ClassConflict>& conflicts,
    std::string_view conflictingClassName
    )
{
    int count = 0;
    for (const ClassConflict& conflict : conflicts)
    {
        if (conflict.conflictingClassName == conflictingClassName)
        {
            ++count;
        }
    }
    return count;
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineClassScheduleServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    ClassRepository classes(database);
    TeacherService teachers(database);
    ClassInfoService classInfo(database);
    ClassScheduleService schedule(database);
    bool passed = true;

    const auto alphaId = classes.create("Alpha");
    const auto noInfoId = classes.create("NoInfo");
    const auto testingId = classes.create("Testing");
    const auto zuluId = classes.create("Zulu");
    const auto alphaTeacherId = teachers.create(teacher("Alpha Teacher"));
    const auto zuluTeacherId = teachers.create(teacher("Zulu Teacher"));
    passed &= expect(
        alphaId && noInfoId && testingId && zuluId
            && alphaTeacherId && zuluTeacherId,
        "schedule fixtures could not be created"
        );
    if (!alphaId || !noInfoId || !testingId || !zuluId
        || !alphaTeacherId || !zuluTeacherId)
    {
        return 1;
    }

    passed &= expect(
        database.execute(
            "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{*testingId}},
                SqliteValue{std::string("Testing room")}
            }
            ).has_value(),
        "testing-class fixture could not be marked"
        );

    passed &= expect(
        classInfo.save(info(
            *alphaId,
            *alphaTeacherId,
            {ClassTime{"Monday", "4:00 PM", "4:50 PM"}},
            {ClassTime{"Wednesday", "9:00 AM", "9:50 AM"}}
            )).has_value(),
        "alpha class information could not be saved"
        );
    passed &= expect(
        classInfo.save(info(
            *zuluId,
            *zuluTeacherId,
            {ClassTime{"Monday", "4:25 PM", "5:15 PM"}},
            {ClassTime{"Tuesday", "10:00 AM", "11:00 AM"}}
            )).has_value(),
        "zulu class information could not be saved"
        );
    passed &= expect(
        classInfo.save(info(
            *testingId,
            *alphaTeacherId,
            {ClassTime{"Friday", "4:00 PM", "4:50 PM"}}
            )).has_value(),
        "testing class information could not be saved"
        );

    const auto assignments = schedule.loadClassTeacherAssignments();
    passed &= expect(
        assignments
            && assignments->size() == 3
            && assignments->at(0).classId == *alphaId
            && assignments->at(0).teacherId == *alphaTeacherId
            && assignments->at(1).classId == *noInfoId
            && assignments->at(1).teacherId == -1
            && assignments->at(2).classId == *zuluId
            && assignments->at(2).teacherId == *zuluTeacherId,
        "class teacher assignment snapshot did not filter or order classes"
        );

    const auto scheduleInfos = schedule.loadScheduleClassInfos();
    const ClassInfo* alpha = scheduleInfos
        ? findInfo(*scheduleInfos, *alphaId)
        : nullptr;
    const ClassInfo* noInfo = scheduleInfos
        ? findInfo(*scheduleInfos, *noInfoId)
        : nullptr;
    const ClassInfo* zulu = scheduleInfos
        ? findInfo(*scheduleInfos, *zuluId)
        : nullptr;
    passed &= expect(
        scheduleInfos
            && scheduleInfos->size() == 3
            && alpha != nullptr
            && alpha->teacherId == *alphaTeacherId
            && alpha->teacherEn == "Alpha Teacher"
            && alpha->classTimes.size() == 1
            && alpha->intensiveTimes.size() == 1
            && noInfo != nullptr
            && noInfo->teacherId == -1
            && noInfo->classColor == "#FFFFFF"
            && noInfo->fontColor == "#000000"
            && noInfo->classTimes.empty()
            && zulu != nullptr
            && zulu->teacherPreferredName == ""
            && zulu->classTimes.size() == 1
            && zulu->classTimes.front().startTime == "4:25 PM",
        "schedule class-information snapshot did not preserve joined data or defaults"
        );

    const std::vector<ClassTime> candidates{
        ClassTime{"Monday", "4:00 PM", "4:50 PM"},
        ClassTime{"Monday", "4:40 PM", "5:20 PM"}
    };
    const auto conflicts = schedule.getClassTimeConflicts(
        *alphaId,
        candidates,
        ScheduleType::Regular
        );
    passed &= expect(
        conflicts
            && conflicts->size() == 3
            && countConflictsFor(*conflicts, "Alpha") == 1
            && countConflictsFor(*conflicts, "Zulu") == 2,
        "schedule conflict detection did not report candidate and stored overlaps"
        );
    if (conflicts && !conflicts->empty())
    {
        passed &= expect(
            conflicts->front().classId == *alphaId
                && conflicts->front().className == "Alpha"
                && conflicts->front().day == "Monday"
                && conflicts->front().conflictingClassName == "Alpha",
            "same-class conflict did not retain the candidate row details"
            );
    }

    const auto intensiveConflicts = schedule.getClassTimeConflicts(
        *alphaId,
        {ClassTime{"Tuesday", "10:30 AM", "10:45 AM"}},
        ScheduleType::Intensive
        );
    passed &= expect(
        intensiveConflicts
            && intensiveConflicts->size() == 1
            && intensiveConflicts->front().conflictingClassName == "Zulu",
        "intensive schedule conflict detection did not select the intensive table"
        );

    const auto invalidCandidate = schedule.getClassTimeConflicts(
        *alphaId,
        {ClassTime{"Not a day", "not a time", "not a time"}},
        ScheduleType::Regular
        );
    passed &= expect(
        invalidCandidate && invalidCandidate->empty(),
        "invalid conflict candidates were not safely ignored"
        );

    const auto missing = schedule.getClassTimeConflicts(
        999999,
        {},
        ScheduleType::Regular
        );
    passed &= expect(
        !missing && missing.error().code == ErrorCode::NotFound,
        "missing conflict class did not return a typed not-found error"
        );

    return passed ? 0 : 1;
}

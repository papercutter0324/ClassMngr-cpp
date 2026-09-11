#include "classmngr/engine/sub_prep_class_information.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace classmngr::engine;

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSubPrepClassInformationServiceTests: "
              << message << '\n';
    return false;
}

ClassTime meeting(
    std::string day,
    std::string start
    )
{
    return {std::move(day), std::move(start), "4:50 PM"};
}

SubPrepSourceClass sourceClass(
    int classId,
    const Teacher& teacher,
    std::string grade,
    std::string level,
    std::vector<ClassTime> regularTimes,
    std::vector<ClassTime> intensiveTimes = {}
    )
{
    SubPrepSourceClass source;
    source.classroom.id = classId;
    source.classroom.name = level;
    source.info.classId = classId;
    source.info.teacherId = teacher.id;
    source.info.classGrade = std::move(grade);
    source.info.classLevel = std::move(level);
    source.info.classTimes = std::move(regularTimes);
    source.info.intensiveTimes = std::move(intensiveTimes);
    source.info.notes = "Class " + std::to_string(classId) + " notes";
    source.teacher = teacher;
    source.studentCount = classId;
    return source;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        SubPrepClassInformationService::formatMeetingTimes(
            {
                meeting("Wednesday", "4:00 PM"),
                meeting("Friday", "5:00 PM"),
                meeting("Monday", "4:00 PM"),
                meeting("Monday", "4:00 PM")
            },
            {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday"}
            ) == "MonWed 4pm & Fri 5pm",
        "shared meeting times changed"
        );

    passed &= expect(
        SubPrepClassInformationService::formatMeetingTimes(
            {
                meeting("Monday", "4:05 PM"),
                meeting("Saturday", "10:00 AM"),
                meeting("Wednesday", "not a time")
            },
            {"Monday", "Wednesday"}
            ) == "Mon 4:05pm"
            && SubPrepClassInformationService::formatMeetingTimes(
                {
                    meeting("Monday", "4:05 PM")
                },
                {"Tuesday"}
                ) == "N/A",
        "day and invalid-time filtering changed"
        );

    Teacher alice;
    alice.id = 10;
    alice.teacherEn = "Alice";
    alice.teacherKr = "앨리스";
    alice.preferredName = "Alice Kim";
    alice.notes = "One teacher note";

    Teacher bob;
    bob.id = 20;
    bob.teacherEn = "Bob";

    SubPrepBuildOptions options;
    options.visibleClassIds = {2, 4, 6};
    options.visibleDays = {
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"
    };

    const auto groups = SubPrepClassInformationService::build(
        {
            sourceClass(
                6,
                alice,
                "E6",
                "Poseidon",
                {meeting("Friday", "5:00 PM")}
                ),
            sourceClass(
                4,
                alice,
                "E4",
                "Hercules",
                {meeting("Monday", "4:00 PM")}
                ),
            sourceClass(
                4,
                alice,
                "E4",
                "Hercules",
                {meeting("Monday", "4:00 PM")}
                ),
            sourceClass(
                2,
                bob,
                "M2",
                "Major",
                {meeting("Tuesday", "6:00 PM")}
                ),
            sourceClass(
                99,
                bob,
                "M3",
                "Minor",
                {meeting("Wednesday", "6:00 PM")}
                )
        },
        options
        );

    passed &= expect(
        groups.size() == 2
            && groups.at(0).displayName == "Alice Kim"
            && groups.at(0).classListText == "E4 Hercules / E6 Poseidon"
            && groups.at(0).classes.size() == 2
            && groups.at(0).classes.at(0).classId == 4
            && groups.at(0).classes.at(0).studentCount == 4
            && groups.at(0).classes.at(0).info.notes == "Class 4 notes"
            && groups.at(0).teacher.notes == "One teacher note"
            && groups.at(1).displayName == "Bob"
            && groups.at(1).classes.size() == 1
            && groups.at(1).classes.front().classId == 2,
        "teacher grouping and class ordering changed"
        );

    Teacher susan;
    susan.id = 1;
    susan.teacherEn = "Susan";
    options.visibleClassIds = {7};
    options.visibleDays = {"Wednesday"};
    options.useIntensive = true;
    const auto intensive = SubPrepClassInformationService::build(
        {
            sourceClass(
                7,
                susan,
                "E5",
                "Zeus",
                {meeting("Monday", "4:00 PM")},
                {meeting("Wednesday", "10:00 AM")}
                )
        },
        options
        );
    passed &= expect(
        intensive.size() == 1
            && intensive.front().classes.front().timeText == "Wed 10am",
        "intensive schedule selection changed"
        );

    Teacher koreanOnly;
    koreanOnly.id = 2;
    koreanOnly.teacherKr = "김선생";
    options.visibleClassIds = {8};
    options.visibleDays = {"Monday"};
    options.useIntensive = false;
    auto fallback = SubPrepClassInformationService::build(
        {
            sourceClass(
                8,
                koreanOnly,
                "",
                "",
                {meeting("Monday", "4:00 PM")}
                )
        },
        options
        );
    passed &= expect(
        fallback.size() == 1
            && fallback.front().displayName == "김선생"
            && fallback.front().classListText == "N/A",
        "fallback labels changed"
        );

    return passed ? 0 : 1;
}

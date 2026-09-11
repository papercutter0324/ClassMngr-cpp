#include "classmngr/engine/sub_prep_package.h"

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace classmngr::engine;

namespace
{
CalendarDate date(int year, unsigned month, unsigned day)
{
    return {
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
}

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }
    std::cerr << "ClassMngrEngineSubPrepPackageServiceTests: "
              << message << '\n';
    return false;
}

SubPrepPackageSourceClass sourceClass(
    int classId,
    std::string grade,
    std::string level,
    std::string teacherName,
    std::vector<ClassTime> regular,
    std::vector<ClassTime> intensive = {}
    )
{
    SubPrepPackageSourceClass source;
    source.classroom.id = classId;
    source.classroom.name = level;
    source.info.classId = classId;
    source.info.teacherId = classId + 10;
    source.info.classGrade = std::move(grade);
    source.info.classLevel = std::move(level);
    source.info.classTimes = std::move(regular);
    source.info.intensiveTimes = std::move(intensive);
    source.teacher.id = classId + 10;
    source.teacher.teacherEn = std::move(teacherName);
    return source;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        SubPrepPackageService::safePathComponent(
            "E4 / Susan: 4:00"
            ) == "E4 - Susan. 4.00",
        "unsafe path characters changed"
        );
    passed &= expect(
        SubPrepPackageService::safePathComponent("CON") == "_CON"
            && SubPrepPackageService::safePathComponent("  \xE2\x80\xA2  ")
                == "-",
        "reserved-name or bullet handling changed"
        );
    passed &= expect(
        SubPrepPackageService::datedFolderName(
            "Alex",
            {date(2026, 7, 20), date(2026, 7, 22)}
            ) == "Alex (20 - 22 Jul 2026)"
            && SubPrepPackageService::datedFolderName(
                "Alex",
                {date(2026, 12, 31), date(2027, 1, 1)}
                ) == "Alex (31 Dec 2026 - 01 Jan 2027)",
        "date-range formatting changed"
        );

    const std::vector<SubPrepScheduleCell> schedule{
        {"Tuesday", {42, 7, 42}},
        {"Monday", {9}},
        {"Wednesday", {7}}
    };
    passed &= expect(
        SubPrepPackageService::classIdsForDays(
            schedule,
            {"Tuesday", "Wednesday"}
            ) == std::vector<int>{42, 7},
        "selected-day class ids changed"
        );

    SubPrepPackageBuildOptions options;
    options.userName = "Alex";
    options.selectedDates = {date(2026, 7, 21)};
    options.classIds = {2, 1, 2};
    options.rosterTemplate = SubPrepRosterTemplate::PerClassWithExtraInfo;

    const auto plan = SubPrepPackageService::build(
        {
            sourceClass(
                1,
                "E6",
                "Poseidon",
                "Susan",
                {{"Tuesday", "5:00 PM", "5:50 PM"}}
                ),
            sourceClass(
                2,
                "E4",
                "Hercules",
                "Susan",
                {{"Tuesday", "4:00 PM", "4:50 PM"}}
                ),
            sourceClass(
                3,
                "E3",
                "Hidden",
                "Susan",
                {{"Monday", "4:00 PM", "4:50 PM"}}
                )
        },
        options
        );
    passed &= expect(
        plan.has_value()
            && plan->folderName == "Alex (21 Jul 2026)"
            && plan->classes.size() == 2
            && plan->classes.at(0).classroom.id == 2
            && plan->classes.at(1).classroom.id == 1
            && plan->classes.at(0).folderName
                != plan->classes.at(1).folderName
            && plan->relativeDocumentPaths
                == std::vector<std::string>{
                    "Sub Prep.pdf",
                    plan->classes.at(0).folderName + "/Roster.pdf",
                    plan->classes.at(1).folderName + "/Roster.pdf"
                },
        "package class filtering, ordering, or document paths changed"
        );

    SubPrepPackageBuildOptions intensive = options;
    intensive.classIds = {4};
    intensive.useIntensiveSchedule = true;
    intensive.rosterTemplate = SubPrepRosterTemplate::Daily;
    const auto intensivePlan = SubPrepPackageService::build(
        {
            sourceClass(
                4,
                "E5",
                "Zeus",
                "Emma",
                {{"Tuesday", "4:00 PM", "4:50 PM"}},
                {{"Monday", "10:00 AM", "10:50 AM"}}
                )
        },
        intensive
        );
    passed &= expect(
        !intensivePlan.has_value()
            && intensivePlan.error().code == ErrorCode::InvalidArgument,
        "intensive day filtering changed"
        );

    intensive.selectedDates = {date(2026, 7, 20)};
    const auto validIntensive = SubPrepPackageService::build(
        {
            sourceClass(
                4,
                "E5",
                "Zeus",
                "Emma",
                {{"Tuesday", "4:00 PM", "4:50 PM"}},
                {{"Monday", "10:00 AM", "10:50 AM"}}
                )
        },
        intensive
        );
    passed &= expect(
        validIntensive.has_value()
            && validIntensive->relativeDocumentPaths
                == std::vector<std::string>{
                    "Sub Prep.pdf",
                    "Rosters - Daily.pdf"
                },
        "intensive package or aggregate roster path changed"
        );

    return passed ? 0 : 1;
}

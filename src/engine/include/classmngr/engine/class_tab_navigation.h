#pragma once

#include "classmngr/engine/class_info.h"

#include <string>
#include <vector>

namespace classmngr::engine
{

class ClassTabNavigationService final
{
public:
    static constexpr int FlatClassThreshold = 6;

    enum class Mode
    {
        Flat,
        GradeGrouped
    };

    enum class GroupingPolicy
    {
        Adaptive,
        AlwaysGradeGrouped
    };

    enum class ScheduleSource
    {
        Regular,
        Intensive
    };

    enum class VisibilityScope
    {
        AllClasses,
        ActiveSchedule
    };

    struct DayFilter
    {
        std::vector<std::string> selectedDays;
        ScheduleSource scheduleSource{ScheduleSource::Regular};
        VisibilityScope visibilityScope{VisibilityScope::AllClasses};
    };

    struct ClassEntry
    {
        int classId{-1};
        std::string classroomName;
        std::string grade;
        std::string level;
        std::vector<ClassTime> regularTimes;
        std::vector<ClassTime> intensiveTimes;
        std::string teacherEn;
        std::string teacherKr;
    };

    struct ClassTab
    {
        int classId{-1};
        std::string label;
    };

    struct GradeGroup
    {
        std::string grade;
        std::string label;
        std::vector<ClassTab> classes;
    };

    struct Model
    {
        Mode mode{Mode::Flat};
        std::vector<ClassTab> allClasses;
        std::vector<ClassTab> flatClasses;
        std::vector<GradeGroup> gradeGroups;
    };

    struct Labels
    {
        std::string other{"Other"};
        std::string intensive{"Int"};
        std::string noTime{"No time"};
        std::string classFallback{"Class %1"};
    };

    [[nodiscard]] static Model build(
        const std::vector<ClassEntry>& entries,
        GroupingPolicy groupingPolicy = GroupingPolicy::Adaptive,
        const DayFilter& dayFilter = DayFilter{
            {},
            ScheduleSource::Regular,
            VisibilityScope::AllClasses
        },
        const Labels& labels = Labels{
            "Other",
            "Int",
            "No time",
            "Class %1"
        }
        );
};

} // namespace classmngr::engine

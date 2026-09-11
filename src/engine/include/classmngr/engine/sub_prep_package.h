#pragma once

#include "classmngr/engine/academic_calendar.h"
#include "classmngr/engine/class_info.h"
#include "classmngr/engine/classroom.h"
#include "classmngr/engine/result.h"
#include "classmngr/engine/teacher.h"

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

enum class SubPrepRosterTemplate
{
    ByDay,
    Daily,
    PerClassWithExtraInfo
};

struct SubPrepScheduleCell
{
    std::string day;
    std::vector<int> classIds;
};

struct SubPrepPackageSourceClass
{
    Classroom classroom;
    ClassInfo info;
    Teacher teacher;
};

struct SubPrepPackageClass
{
    Classroom classroom;
    ClassInfo info;
    Teacher teacher;
    std::string displayName;
    std::string folderName;
};

struct SubPrepPackageBuildOptions
{
    std::string userName;
    std::vector<CalendarDate> selectedDates;
    std::vector<int> classIds;
    bool useIntensiveSchedule = false;
    SubPrepRosterTemplate rosterTemplate = SubPrepRosterTemplate::ByDay;
};

struct SubPrepPackagePlan
{
    std::string folderName;
    std::vector<SubPrepPackageClass> classes;
    std::vector<std::string> relativeDocumentPaths;
};

class SubPrepPackageService final
{
public:
    [[nodiscard]] static std::string safePathComponent(
        std::string_view value,
        std::string_view fallback = "Sub Prep"
        );

    [[nodiscard]] static std::string datedFolderName(
        std::string_view userName,
        const std::vector<CalendarDate>& selectedDates
        );

    [[nodiscard]] static std::vector<std::string> selectedDayNames(
        const std::vector<CalendarDate>& selectedDates
        );

    [[nodiscard]] static std::vector<int> classIdsForDays(
        const std::vector<SubPrepScheduleCell>& schedule,
        const std::vector<std::string>& selectedDays
        );

    [[nodiscard]] static std::string rosterDocumentFileName(
        SubPrepRosterTemplate rosterTemplate
        );

    [[nodiscard]] static Result<SubPrepPackagePlan> build(
        const std::vector<SubPrepPackageSourceClass>& sourceClasses,
        const SubPrepPackageBuildOptions& options
        );
};

} // namespace classmngr::engine

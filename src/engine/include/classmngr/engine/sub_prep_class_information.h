#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/classroom.h"
#include "classmngr/engine/teacher.h"

#include <string>
#include <vector>

namespace classmngr::engine
{

struct SubPrepSourceClass
{
    Classroom classroom;
    ClassInfo info;
    Teacher teacher;
    int studentCount = 0;
};

struct SubPrepClassDetails
{
    int classId = -1;
    ClassInfo info;
    int studentCount = 0;
    std::string classLabel;
    std::string timeText;
};

struct SubPrepTeacherGroup
{
    Teacher teacher;
    std::string displayName;
    std::string classListText;
    std::vector<SubPrepClassDetails> classes;
};

struct SubPrepBuildOptions
{
    std::vector<int> visibleClassIds;
    std::vector<std::string> visibleDays;
    bool useIntensive = false;
};

class SubPrepClassInformationService final
{
public:
    // Meeting labels use stable English day tokens; presentation adapters may
    // localize those tokens after receiving the renderer-neutral model.
    [[nodiscard]] static std::string formatMeetingTimes(
        const std::vector<ClassTime>& times,
        const std::vector<std::string>& visibleDays
        );

    [[nodiscard]] static std::vector<SubPrepTeacherGroup> build(
        const std::vector<SubPrepSourceClass>& sourceClasses,
        const SubPrepBuildOptions& options
        );
};

} // namespace classmngr::engine

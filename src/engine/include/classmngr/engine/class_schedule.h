#pragma once

#include "classmngr/engine/class_info.h"

#include <string>

namespace classmngr::engine
{

enum class ScheduleType
{
    Regular,
    Intensive
};

struct ClassTeacherAssignment
{
    int classId = -1;
    int teacherId = -1;
};

struct ClassScheduleEntry
{
    int classId = -1;
    std::string className;
    ClassTime time;
};

struct ClassConflict
{
    int classId = -1;
    std::string className;
    std::string day;
    std::string startTime;
    std::string endTime;
    std::string conflictingClassName;
};

} // namespace classmngr::engine

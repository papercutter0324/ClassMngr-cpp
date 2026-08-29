#pragma once

#include <string>
#include <vector>

namespace classmngr::engine
{

struct ClassTime
{
    std::string day = "Monday";
    std::string startTime = "4:00 PM";
    std::string endTime = "4:50 PM";
};

struct ClassInfo
{
    int classId = -1;
    int teacherId = -1;

    // These fields are populated by the read model's teacher join.  They are
    // intentionally part of the portable model so UI adapters do not need to
    // issue their own teacher lookup while rendering a class editor.
    std::string teacherKr;
    std::string teacherEn;
    std::string teacherPreferredName;
    std::string roomNumber;
    std::string wifiName;
    std::string wifiPassword;
    std::string internetType;
    std::string zoomId;
    std::string zoomPassword;
    std::string projectionType;

    std::string classGrade;
    std::string classLevel;
    std::string readingBook;
    std::string essayBook;
    std::string classColor = "#FFFFFF";
    std::string fontColor = "#000000";

    std::vector<ClassTime> classTimes;
    std::vector<ClassTime> intensiveTimes;

    std::string notes;
    std::string timeFillerActivities;
};

} // namespace classmngr::engine

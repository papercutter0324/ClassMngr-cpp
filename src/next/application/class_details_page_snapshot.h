#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// Schedule text is copied exactly as it is stored. Keeping the original text
// here lets legacy rows cross the application boundary without parsing or
// dropping malformed values.
struct ClassDetailsPageScheduleRow final
{
    std::string day;
    std::string startTime;
    std::string endTime;

    friend bool operator==(
        const ClassDetailsPageScheduleRow&,
        const ClassDetailsPageScheduleRow&
        ) = default;
};

struct ClassDetailsPageFields final
{
    std::string classGrade;
    std::string classLevel;
    std::string readingBook;
    std::string essayBook;
    std::string classColor{"#FFFFFF"};
    std::string fontColor{"#000000"};
    std::vector<ClassDetailsPageScheduleRow> regularSchedule;
    std::vector<ClassDetailsPageScheduleRow> intensiveSchedule;

    friend bool operator==(
        const ClassDetailsPageFields&,
        const ClassDetailsPageFields&
        ) = default;
};

// One source's value or its diagnostic. The page may use successful sources
// while rendering the documented fallback for another failed source.
struct ClassDetailsPageReadSnapshot final
{
    Domain::ClassId classId;
    Domain::Result<ClassDetailsPageFields> classFields;
    Domain::Result<std::string> teacherDisplayName;
    Domain::Result<int> studentCount;
};

} // namespace ClassMngr::Next::Application

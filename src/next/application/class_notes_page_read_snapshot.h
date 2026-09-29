#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassNotesPageScheduleRow final
{
    std::u16string day;
    std::u16string startTime;

    friend bool operator==(
        const ClassNotesPageScheduleRow&,
        const ClassNotesPageScheduleRow&
        ) = default;
};

struct ClassNotesPageFields final
{
    std::u16string classGrade;
    std::u16string classLevel;
    std::vector<ClassNotesPageScheduleRow> regularSchedule;
    std::u16string notes;
    std::u16string timeFillerActivities;

    friend bool operator==(
        const ClassNotesPageFields&,
        const ClassNotesPageFields&
        ) = default;
};

struct ClassNotesPageReadSnapshot final
{
    Domain::ClassId classId;
    Domain::Result<ClassNotesPageFields> classFields;
    Domain::Result<std::u16string> teacherDisplayName;
};

} // namespace ClassMngr::Next::Application

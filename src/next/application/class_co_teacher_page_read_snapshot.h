#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassCoTeacherPageScheduleRow final
{
    std::u16string day;
    std::u16string startTime;

    friend bool operator==(
        const ClassCoTeacherPageScheduleRow&,
        const ClassCoTeacherPageScheduleRow&
        ) = default;
};

struct ClassCoTeacherPageFields final
{
    std::optional<Domain::TeacherId> selectedTeacherId;
    std::u16string classGrade;
    std::u16string classLevel;
    std::vector<ClassCoTeacherPageScheduleRow> regularSchedule;

    friend bool operator==(
        const ClassCoTeacherPageFields&,
        const ClassCoTeacherPageFields&
        ) = default;
};

struct ClassCoTeacherPageReadSnapshot final
{
    Domain::ClassId classId;
    Domain::Result<ClassCoTeacherPageFields> classFields;
    Domain::Result<std::u16string> teacherDisplayName;
};

} // namespace ClassMngr::Next::Application

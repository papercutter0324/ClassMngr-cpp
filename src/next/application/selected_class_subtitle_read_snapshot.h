#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SelectedClassSubtitleScheduleRow final
{
    std::u16string day;
    std::u16string startTime;

    friend bool operator==(
        const SelectedClassSubtitleScheduleRow&,
        const SelectedClassSubtitleScheduleRow&
        ) = default;
};

// This projection contains the class values consumed by sidebar naming and
// speaking-evaluation report context.
struct SelectedClassSubtitleFields final
{
    std::u16string classGrade;
    std::u16string classLevel;
    std::vector<SelectedClassSubtitleScheduleRow> regularSchedule;

    friend bool operator==(
        const SelectedClassSubtitleFields&,
        const SelectedClassSubtitleFields&
        ) = default;
};

// These are the Teacher values used by preferredDisplayName() and speaking-
// evaluation report headers.
struct SelectedClassSubtitleTeacherFields final
{
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string preferredRomanization;
    std::u16string preferredName;

    friend bool operator==(
        const SelectedClassSubtitleTeacherFields&,
        const SelectedClassSubtitleTeacherFields&
        ) = default;
};

// Class details and the optional assigned teacher are separate outcomes so a
// failed teacher lookup cannot discard details that were read successfully.
struct SelectedClassSubtitleReadSnapshot final
{
    Domain::ClassId classId;
    Domain::Result<SelectedClassSubtitleFields> classFields;
    Domain::Result<std::optional<SelectedClassSubtitleTeacherFields>>
        assignedTeacher;
};

} // namespace ClassMngr::Next::Application

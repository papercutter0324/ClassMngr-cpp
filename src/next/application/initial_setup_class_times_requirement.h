#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/domain/schedule_time.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// Initial Setup requires at least one regular or intensive schedule row.
[[nodiscard]] constexpr bool initialSetupClassTimesRequirementMet(
    bool hasRegularRows,
    bool hasIntensiveRows
    ) noexcept
{
    return hasRegularRows || hasIntensiveRows;
}

struct InitialSetupClassTimesSaveRequest final
{
    Domain::ClassId classId;
    Domain::TeacherId teacherId;
    std::u16string classColor;
    std::u16string fontColor;
    std::u16string classGrade;
    std::u16string classLevel;
    std::u16string readingBook;
    std::u16string essayBook;
    std::vector<Domain::ScheduleTime> regularTimes;
    std::vector<Domain::ScheduleTime> intensiveTimes;
};

struct InitialSetupClassTimes final
{
    std::vector<Domain::ScheduleTime> regularTimes;
    std::vector<Domain::ScheduleTime> intensiveTimes;
};

using InitialSetupClassTimesSaveResult = Domain::Result<void>;

} // namespace ClassMngr::Next::Application

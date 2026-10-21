#pragma once

#include "next/domain/course.h"

namespace ClassMngr::Next::Application
{

[[nodiscard]] constexpr bool scheduleTestingSuppressesGradeBand(
    const Domain::CourseGradeBand gradeBand,
    const bool testingAffectsM1
    ) noexcept
{
    switch (gradeBand)
    {
    case Domain::CourseGradeBand::M2:
    case Domain::CourseGradeBand::M3:
        return true;
    case Domain::CourseGradeBand::M1:
        return testingAffectsM1;
    case Domain::CourseGradeBand::E4:
    case Domain::CourseGradeBand::E5:
    case Domain::CourseGradeBand::E6:
    case Domain::CourseGradeBand::Other:
        return false;
    }

    // Unrecognized enum values do not suppress schedule entries.
    return false;
}

} // namespace ClassMngr::Next::Application

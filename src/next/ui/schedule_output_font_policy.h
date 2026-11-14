#pragma once

#include "next/ui/output_font_family_policy.h"

namespace ClassMngr::Next::Ui
{

[[nodiscard]] inline int resolveScheduleOutputEnglishPointSize(
    const int inheritedPointSize,
    const bool koreanGlobalFont,
    const int outputFallbackPointSize
    )
{
    const int inheritedSchedulePointSize = inheritedPointSize
        - (koreanGlobalFont ? 1 : 0);
    return inheritedSchedulePointSize > 0
        ? inheritedSchedulePointSize : outputFallbackPointSize;
}

}  // namespace ClassMngr::Next::Ui

#pragma once

#include "next/application/schedule_view_projection.h"

namespace ClassMngr::Next::Application
{

// Selects one complete navigation-metadata value. A non-null all-week
// candidate represents an existing class record, even when its contained
// label or order values are unusable.
template <typename Metadata>
[[nodiscard]] const Metadata& selectSubPrepNavigationMetadata(
    const ScheduleViewMode requestMode,
    const Metadata* regularAllWeek,
    const Metadata* intensiveAllWeek,
    const Metadata& selectedMode
    ) noexcept
{
    if (regularAllWeek)
    {
        return *regularAllWeek;
    }
    if (requestMode == ScheduleViewMode::Intensive && intensiveAllWeek)
    {
        return *intensiveAllWeek;
    }
    return selectedMode;
}

} // namespace ClassMngr::Next::Application

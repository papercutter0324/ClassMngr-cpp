#pragma once

#include "next/application/schedule_testing_layout_clear_port.h"

namespace ClassMngr::Next::Application
{

// Preserve the separate availability and clear operations used by Preferences
// while keeping session and repository access in Platform.
class ScheduleTestingLayoutClearUseCase final
{
public:
    [[nodiscard]] static bool isAvailable(
        const ScheduleTestingLayoutClearPort& port
        )
    {
        return port.isAvailable();
    }

    [[nodiscard]] static ScheduleTestingLayoutClearResult execute(
        const ScheduleTestingLayoutClearPort& port
        )
    {
        return port.clearTestingLayout();
    }
};

} // namespace ClassMngr::Next::Application

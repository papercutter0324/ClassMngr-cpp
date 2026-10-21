#include "next/application/schedule_display_mode_source_policy.h"

#include <cstdlib>

using ClassMngr::Next::Application::ScheduleDisplayMode;
using ClassMngr::Next::Application::scheduleDisplayModeUsesIntensiveSource;

int main()
{
    if (scheduleDisplayModeUsesIntensiveSource(ScheduleDisplayMode::Regular)
        || !scheduleDisplayModeUsesIntensiveSource(
            ScheduleDisplayMode::Intensive
            )
        || scheduleDisplayModeUsesIntensiveSource(ScheduleDisplayMode::Testing)
        || scheduleDisplayModeUsesIntensiveSource(
            static_cast<ScheduleDisplayMode>(-1)
            ))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#pragma once

#include "next/application/calendar_event_repeat_occurrence_plan.h"
#include "next/application/calendar_event_series_create_port.h"

#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Application
{

// Own the decision to plan a repeat series before asking persistence to
// create it. Invalid seeds and repeat settings therefore never reach the port.
class CalendarEventSeriesCreateUseCase final
{
public:
    [[nodiscard]] static CalendarEventSeriesCreateResult execute(
        CalendarEventSeriesCreatePort& createPort,
        const CalendarEventSaveRequest& seed,
        std::string repeatSeriesId,
        const CalendarEventRepeatFrequency frequency,
        const std::string_view untilDate
        )
    {
        const Domain::Result<CalendarEventSeriesCreateRequest> plannedSeries =
            planCalendarEventRepeatOccurrences(
                seed,
                std::move(repeatSeriesId),
                frequency,
                untilDate
                );
        if (!plannedSeries)
        {
            return CalendarEventSeriesCreateResult::failure(
                plannedSeries.error()
                );
        }

        return createPort.createRepeatSeries(plannedSeries.value());
    }
};

} // namespace ClassMngr::Next::Application

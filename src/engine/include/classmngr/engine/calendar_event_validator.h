#pragma once

#include "classmngr/engine/calendar_event.h"
#include "classmngr/engine/validation_result.h"

#include <cstddef>
#include <vector>

namespace classmngr::engine
{

class CalendarEventValidator final
{
public:
    static constexpr int MaximumTitleLength = 255;
    static constexpr int MaximumRepeatSeriesIdLength = 128;
    static constexpr int MaximumRepeatOccurrences = 366;

    // Text limits count Unicode code points in well-formed UTF-8.  Malformed
    // UTF-8 is counted byte-wise so validation remains deterministic without
    // changing the supplied string.
    [[nodiscard]] static CalendarEvent normalized(
        const CalendarEvent& event
        );

    [[nodiscard]] static ValidationResult validate(
        const CalendarEvent& event
        );

    [[nodiscard]] static ValidationResult validateRecurrence(
        const CalendarEvent& event,
        CalendarEventRepeatFrequency frequency,
        const CalendarDate& untilDate
        );

    [[nodiscard]] static ValidationResult validateSeries(
        const std::vector<CalendarEvent>& events
        );

    [[nodiscard]] static int estimatedRepeatOccurrences(
        const CalendarDate& startDate,
        const CalendarDate& endDate,
        CalendarEventRepeatFrequency frequency
        );
};

} // namespace classmngr::engine

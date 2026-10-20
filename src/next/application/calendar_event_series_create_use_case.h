#pragma once

#include "next/application/calendar_event_repeat_occurrence_plan.h"
#include "next/application/calendar_event_series_create_port.h"
#include "next/domain/domain_types.h"

#include <optional>
#include <string>

namespace ClassMngr::Next::Application
{

// Preserve the dialog's raw cutoff text until frequency and seed validation
// have completed. The UseCase converts it to a domain date before planning.
struct CalendarEventSeriesCreateInput final
{
    CalendarEventSaveRequest seed;
    std::optional<Domain::CalendarEventSeriesId> repeatSeriesId;
    CalendarEventRepeatFrequency frequency =
        CalendarEventRepeatFrequency::Daily;
    std::string untilDate;
};

// Own the decision to plan a repeat series before asking persistence to
// create it. Invalid seeds and repeat settings therefore never reach the port.
class CalendarEventSeriesCreateUseCase final
{
public:
    [[nodiscard]] static CalendarEventSeriesCreateResult execute(
        CalendarEventSeriesCreatePort& createPort,
        const CalendarEventSeriesCreateInput& input
        )
    {
        const Domain::Result<void> seedAndFrequencyValidation =
            CalendarEventRepeatOccurrencePlanDetail::
                validateSeedAndFrequency(input.seed, input.frequency);
        if (!seedAndFrequencyValidation)
        {
            return CalendarEventSeriesCreateResult::failure(
                seedAndFrequencyValidation.error()
                );
        }

        const std::optional<Domain::GregorianDate> finalStartDate =
            Domain::GregorianDate::fromIsoString(input.untilDate);
        if (!finalStartDate)
        {
            return CalendarEventSeriesCreateResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Calendar repeat seed and until dates must be "
                    "canonical ISO dates.",
                .recoverable = false
            });
        }

        const Domain::Result<CalendarEventSeriesCreateRequest> plannedSeries =
            planCalendarEventRepeatOccurrences(
                input.seed,
                input.repeatSeriesId,
                input.frequency,
                *finalStartDate
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

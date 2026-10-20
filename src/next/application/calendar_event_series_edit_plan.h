#pragma once

#include "next/application/calendar_event_series_edit_port.h"
#include "next/domain/gregorian_date.h"
#include "next/domain/domain_types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// The platform adapter supplies the occurrence identity and persisted start
// date in repository order. The planner deliberately does not select or sort
// occurrences; the repository query already returns the requested suffix.
struct CalendarEventSeriesEditOccurrenceSnapshot final
{
    Domain::CalendarEventId eventId;
    std::optional<Domain::GregorianDate> startDate;

    friend bool operator==(
        const CalendarEventSeriesEditOccurrenceSnapshot&,
        const CalendarEventSeriesEditOccurrenceSnapshot&
        ) = default;
};

// A complete set of values for updating one selected occurrence. The ID lets
// the adapter verify that each update is applied to its original event.
struct CalendarEventSeriesEditOccurrenceUpdate final
{
    Domain::CalendarEventId eventId;
    Domain::GregorianDate startDate;
    Domain::GregorianDate endDate;
    Domain::CalendarEventSeriesId repeatSeriesId;
    std::string title;
    Domain::CalendarEventType eventType = Domain::CalendarEventType::Other;
    Domain::CalendarEventTimeStatus timeStatus =
        Domain::CalendarEventTimeStatus::Timed;
    std::optional<std::string> startTime;
    std::optional<std::string> endTime;
    bool allDay = false;

    friend bool operator==(
        const CalendarEventSeriesEditOccurrenceUpdate&,
        const CalendarEventSeriesEditOccurrenceUpdate&
        ) = default;
};

using CalendarEventSeriesEditPlan =
    std::vector<CalendarEventSeriesEditOccurrenceUpdate>;
using CalendarEventSeriesEditPlanResult =
    Domain::Result<CalendarEventSeriesEditPlan>;

namespace CalendarEventSeriesEditPlanDetail
{

using Date = Domain::GregorianDate;
using RawDayNumber = std::int64_t;

[[nodiscard]] inline CalendarEventSeriesEditPlanResult technicalFailure(
    const char* message
    )
{
    return CalendarEventSeriesEditPlanResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = message,
        .recoverable = false
    });
}

} // namespace CalendarEventSeriesEditPlanDetail

// Plans date and request-field updates for an already selected repository
// suffix. The selection's order is preserved exactly. Invalid persisted dates
// and shifted dates outside the supported Gregorian range are technical
// failures because they describe source data or an unrepresentable update.
[[nodiscard]] inline CalendarEventSeriesEditPlanResult
planCalendarEventSeriesEditSuffix(
    const CalendarEventSeriesEditRequest& request,
    const std::vector<CalendarEventSeriesEditOccurrenceSnapshot>& occurrences
    )
{
    const Domain::Result<void> validation = request.validate();
    if (!validation)
    {
        return CalendarEventSeriesEditPlanResult::failure(validation.error());
    }

    using namespace CalendarEventSeriesEditPlanDetail;
    const RawDayNumber startDateOffset =
        request.editedStartDate.dayNumber().rawValue()
        - request.startDate.dayNumber().rawValue();
    const RawDayNumber durationDays =
        request.editedEndDate.dayNumber().rawValue()
        - request.editedStartDate.dayNumber().rawValue();
    const std::optional<Domain::CalendarEventSeriesId> repeatSeriesId =
        Domain::CalendarEventSeriesId::fromString(
            CalendarEventSeriesEditRequestDetail::trimAscii(
                request.repeatSeriesId->value()
                )
            );
    if (!repeatSeriesId)
    {
        return CalendarEventSeriesEditPlanResult::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Calendar repeat-series identifier must be non-blank and bounded.",
            .recoverable = false
        });
    }
    CalendarEventSeriesEditPlan plan;
    plan.reserve(occurrences.size());
    for (const CalendarEventSeriesEditOccurrenceSnapshot& occurrence : occurrences)
    {
        if (!occurrence.startDate)
        {
            return technicalFailure(
                "A calendar repeat-series occurrence has an invalid start date."
                );
        }

        const RawDayNumber shiftedStartDate =
            occurrence.startDate->dayNumber().rawValue() + startDateOffset;
        const RawDayNumber shiftedEndDate = shiftedStartDate + durationDays;
        const std::optional<Date> shiftedStart =
            Date::fromDayNumber(Date::DayNumber{shiftedStartDate});
        const std::optional<Date> shiftedEnd = Date::fromDayNumber(
            Date::DayNumber{shiftedEndDate}
            );
        if (!shiftedStart || !shiftedEnd)
        {
            return technicalFailure(
                "A calendar repeat-series edit would move an occurrence outside the supported date range."
                );
        }

        plan.push_back({
            .eventId = occurrence.eventId,
            .startDate = *shiftedStart,
            .endDate = *shiftedEnd,
            .repeatSeriesId = *repeatSeriesId,
            .title = request.title,
            .eventType = request.eventType,
            .timeStatus = request.timeStatus,
            .startTime = request.startTime,
            .endTime = request.endTime,
            .allDay = request.allDay
        });
    }

    return CalendarEventSeriesEditPlanResult::success(std::move(plan));
}

} // namespace ClassMngr::Next::Application

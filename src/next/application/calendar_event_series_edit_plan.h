#pragma once

#include "next/application/calendar_event_series_edit_port.h"
#include "next/domain/gregorian_date.h"

#include <cstddef>
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
    int eventId = -1;
    std::string startDate;

    friend bool operator==(
        const CalendarEventSeriesEditOccurrenceSnapshot&,
        const CalendarEventSeriesEditOccurrenceSnapshot&
        ) = default;
};

// A complete set of values for updating one selected occurrence. The ID lets
// the adapter verify that each update is applied to its original event.
struct CalendarEventSeriesEditOccurrenceUpdate final
{
    int eventId = -1;
    std::string startDate;
    std::string endDate;
    std::string repeatSeriesId;
    std::string title;
    std::string eventType;
    std::string timeStatus;
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
using DayNumber = Date::DayNumber;

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
    const std::optional<Date> selectedStartDate =
        Date::fromIsoString(request.startDate);
    const std::optional<Date> editedStartDate =
        Date::fromIsoString(request.editedStartDate);
    const std::optional<Date> editedEndDate =
        Date::fromIsoString(request.editedEndDate);
    if (!selectedStartDate || !editedStartDate || !editedEndDate)
    {
        return CalendarEventSeriesEditPlanResult::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Calendar repeat-series edit dates must be valid canonical ISO dates.",
            .recoverable = false
        });
    }

    const DayNumber startDateOffset =
        editedStartDate->dayNumber() - selectedStartDate->dayNumber();
    const DayNumber durationDays =
        editedEndDate->dayNumber() - editedStartDate->dayNumber();
    const std::string repeatSeriesId(
        CalendarEventSeriesEditRequestDetail::trimAscii(
            request.repeatSeriesId
            )
        );
    CalendarEventSeriesEditPlan plan;
    plan.reserve(occurrences.size());
    for (const CalendarEventSeriesEditOccurrenceSnapshot& occurrence : occurrences)
    {
        const std::optional<Date> sourceStartDate =
            Date::fromIsoString(occurrence.startDate);
        if (!sourceStartDate)
        {
            return technicalFailure(
                "A calendar repeat-series occurrence has an invalid start date."
                );
        }

        const DayNumber shiftedStartDate =
            sourceStartDate->dayNumber() + startDateOffset;
        const DayNumber shiftedEndDate = shiftedStartDate + durationDays;
        const std::optional<Date> shiftedStart =
            Date::fromDayNumber(shiftedStartDate);
        const std::optional<Date> shiftedEnd = Date::fromDayNumber(shiftedEndDate);
        if (!shiftedStart || !shiftedEnd)
        {
            return technicalFailure(
                "A calendar repeat-series edit would move an occurrence outside the supported date range."
                );
        }

        plan.push_back({
            .eventId = occurrence.eventId,
            .startDate = shiftedStart->toIsoString(),
            .endDate = shiftedEnd->toIsoString(),
            .repeatSeriesId = repeatSeriesId,
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

#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// A calendar projection is a bounded event-list snapshot. Query and adapter
// owners must paginate or stage a larger result before create(); the
// projection never becomes an unbounded calendar or recurrence store.
inline constexpr std::size_t kCalendarEventProjectionMaxEvents = 4'096;
inline constexpr std::size_t kCalendarEventSummaryMaxIdentifierLength = 256;
inline constexpr std::size_t kCalendarEventSummaryMaxTitleLength = 256;
inline constexpr std::size_t kCalendarEventSummaryMaxDateLength = 32;
inline constexpr std::size_t kCalendarEventSummaryMaxTimeLength = 32;
inline constexpr std::size_t kCalendarEventSummaryMaxLocationLength = 256;
inline constexpr std::size_t kCalendarEventSummaryMaxNotesLength = 2'048;

// These aliases keep the limits discoverable for callers that name the
// projection rather than the row type; they intentionally share one bound.
inline constexpr std::size_t kCalendarEventProjectionMaxEntries =
    kCalendarEventProjectionMaxEvents;
inline constexpr std::size_t kCalendarEventSummaryMaxEntries =
    kCalendarEventProjectionMaxEvents;
inline constexpr std::size_t kCalendarEventMaxEvents =
    kCalendarEventProjectionMaxEvents;
inline constexpr std::size_t kCalendarEventMaxEntries =
    kCalendarEventProjectionMaxEvents;
inline constexpr std::size_t kCalendarEventMaxIdentifierLength =
    kCalendarEventSummaryMaxIdentifierLength;
inline constexpr std::size_t kCalendarEventMaxTitleLength =
    kCalendarEventSummaryMaxTitleLength;
inline constexpr std::size_t kCalendarEventMaxDateLength =
    kCalendarEventSummaryMaxDateLength;
inline constexpr std::size_t kCalendarEventMaxTimeLength =
    kCalendarEventSummaryMaxTimeLength;
inline constexpr std::size_t kCalendarEventMaxLocationLength =
    kCalendarEventSummaryMaxLocationLength;
inline constexpr std::size_t kCalendarEventMaxNotesLength =
    kCalendarEventSummaryMaxNotesLength;

// A summary contains only event-list metadata. Dates and times are opaque
// adapter-neutral text (ISO-like text is recommended at the boundary), so
// this contract does not parse formats, time zones, or date ordering.
//
// Time policy: an all-day event must omit both optional times. A non-all-day
// event may omit both times when the source has no reliable time, but a
// partial time range is rejected. When present, both times are bounded text.
// Location and notes use empty text as the absent value; non-empty values
// must not be blank or oversized.
struct CalendarEventSummary final
{
    Domain::CalendarEventId id;
    std::optional<Domain::ClassId> classId;
    std::optional<Domain::CampusId> campusId;
    std::string title;
    std::string startDate;
    std::string endDate;
    std::optional<std::string> startTime;
    std::optional<std::string> endTime;
    std::string location;
    std::string notes;
    std::int32_t order = 0;
    bool allDay = false;

    [[nodiscard]] bool hasClass() const noexcept
    {
        return classId.has_value();
    }

    [[nodiscard]] bool hasCampus() const noexcept
    {
        return campusId.has_value();
    }

    [[nodiscard]] bool hasTimeRange() const noexcept
    {
        return startTime.has_value() && endTime.has_value();
    }

    [[nodiscard]] bool isAllDay() const noexcept
    {
        return allDay;
    }

    friend bool operator==(
        const CalendarEventSummary&,
        const CalendarEventSummary&
        ) = default;
};

struct CalendarEventProjectionInput final
{
    std::vector<CalendarEventSummary> events;

    friend bool operator==(
        const CalendarEventProjectionInput&,
        const CalendarEventProjectionInput&
        ) = default;
};

using CalendarEventInput = CalendarEventProjectionInput;
using CalendarEventSummaryInput = CalendarEventProjectionInput;

namespace CalendarEventProjectionDetail
{

[[nodiscard]] inline bool isBlank(
    const std::string_view value
    ) noexcept
{
    return value.empty()
        || std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
}

[[nodiscard]] inline bool isRequiredText(
    const std::string_view value,
    const std::size_t maxLength
    ) noexcept
{
    return !isBlank(value) && value.size() <= maxLength;
}

[[nodiscard]] inline bool isOptionalText(
    const std::string_view value,
    const std::size_t maxLength
    ) noexcept
{
    return value.empty()
        || (!isBlank(value) && value.size() <= maxLength);
}

[[nodiscard]] inline bool isValidIdentifier(
    const std::string_view value
    ) noexcept
{
    return isRequiredText(value, kCalendarEventSummaryMaxIdentifierLength);
}

[[nodiscard]] inline Domain::OperationError invalidInput(
    const char* message
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    };
}

template <typename TypedId>
[[nodiscard]] inline bool isValidId(
    const TypedId& id
    ) noexcept
{
    return isValidIdentifier(id.value());
}

template <typename TypedId>
[[nodiscard]] inline bool containsId(
    const std::vector<TypedId>& values,
    const TypedId& candidate
    ) noexcept
{
    return std::find(values.cbegin(), values.cend(), candidate)
        != values.cend();
}

[[nodiscard]] inline Domain::Result<void> validateEvent(
    const CalendarEventSummary& event,
    const std::vector<Domain::CalendarEventId>& existingIds
    )
{
    if (!isValidId(event.id))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Calendar event identifier must be non-blank and bounded."
                )
            );
    }

    if (containsId(existingIds, event.id))
    {
        return Domain::Result<void>::failure(
            invalidInput("Calendar event identifiers must be unique.")
            );
    }

    if (event.classId.has_value() && !isValidId(*event.classId))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Optional calendar event class identifier must be non-blank and bounded."
                )
            );
    }

    if (event.campusId.has_value() && !isValidId(*event.campusId))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Optional calendar event campus identifier must be non-blank and bounded."
                )
            );
    }

    if (!isRequiredText(
            event.title,
            kCalendarEventSummaryMaxTitleLength
            )
        || !isRequiredText(
            event.startDate,
            kCalendarEventSummaryMaxDateLength
            )
        || !isRequiredText(
            event.endDate,
            kCalendarEventSummaryMaxDateLength
            )
        || !isOptionalText(
            event.location,
            kCalendarEventSummaryMaxLocationLength
            )
        || !isOptionalText(event.notes, kCalendarEventSummaryMaxNotesLength))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Calendar event title and dates must be non-blank and bounded; location and notes must be empty or bounded."
                )
            );
    }

    if (event.order < 0)
    {
        return Domain::Result<void>::failure(
            invalidInput("Calendar event order must not be negative.")
            );
    }

    const bool hasStartTime = event.startTime.has_value();
    const bool hasEndTime = event.endTime.has_value();
    if (hasStartTime != hasEndTime)
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Calendar event start and end times must be both absent or both present."
                )
            );
    }

    if ((hasStartTime
         && !isRequiredText(
             *event.startTime,
             kCalendarEventSummaryMaxTimeLength
             ))
        || (hasEndTime
            && !isRequiredText(
                *event.endTime,
                kCalendarEventSummaryMaxTimeLength
                )))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Calendar event times must be non-blank and bounded when present."
                )
            );
    }

    if (event.allDay && (hasStartTime || hasEndTime))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "All-day calendar events must omit start and end times."
                )
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateInput(
    const CalendarEventProjectionInput& input
    )
{
    if (input.events.size() > kCalendarEventProjectionMaxEvents)
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Calendar event collection exceeds its bounded limit."
                )
            );
    }

    std::vector<Domain::CalendarEventId> existingIds;
    existingIds.reserve(input.events.size());
    for (const auto& event : input.events)
    {
        const auto validation = validateEvent(event, existingIds);
        if (!validation)
        {
            return validation;
        }

        existingIds.push_back(event.id);
    }

    return Domain::Result<void>::success();
}

}

// This immutable snapshot owns copied event-list metadata only. Once create()
// succeeds, the query/adapter owner may release rich calendar records,
// recurrence graphs, repositories, Qt date/time values, and raw service
// state. Callers receive value copies from lookups and must paginate or stage
// above the projection cap before crossing this boundary.
class CalendarEventProjection final
{
public:
    using Input = CalendarEventProjectionInput;
    using Event = CalendarEventSummary;
    using Summary = CalendarEventSummary;

    CalendarEventProjection() = default;

    [[nodiscard]] static Domain::Result<CalendarEventProjection> create(
        Input input
        )
    {
        const auto validation = CalendarEventProjectionDetail::validateInput(
            input
            );
        if (!validation)
        {
            return Domain::Result<CalendarEventProjection>::failure(
                validation.error()
                );
        }

        return Domain::Result<CalendarEventProjection>::success(
            CalendarEventProjection(std::move(input.events))
            );
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Input& input
        )
    {
        return CalendarEventProjectionDetail::validateInput(input);
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Summary& event
        )
    {
        return CalendarEventProjectionDetail::validateEvent(event, {});
    }

    [[nodiscard]] const std::vector<Event>& events() const noexcept
    {
        return m_events;
    }

    [[nodiscard]] const std::vector<Summary>& summaries() const noexcept
    {
        return m_events;
    }

    [[nodiscard]] const std::vector<Summary>& entries() const noexcept
    {
        return m_events;
    }

    [[nodiscard]] const std::vector<Event>& calendarEvents() const noexcept
    {
        return m_events;
    }

    [[nodiscard]] std::size_t eventCount() const noexcept
    {
        return m_events.size();
    }

    [[nodiscard]] std::size_t count() const noexcept
    {
        return eventCount();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return eventCount();
    }

    [[nodiscard]] std::optional<Event> findEvent(
        const Domain::CalendarEventId& id
        ) const
    {
        const auto event = std::find_if(
            m_events.cbegin(),
            m_events.cend(),
            [&id](const Event& candidate)
            {
                return candidate.id == id;
            }
            );
        if (event == m_events.cend())
        {
            return std::nullopt;
        }

        return *event;
    }

    [[nodiscard]] std::optional<Summary> lookupEvent(
        const Domain::CalendarEventId& id
        ) const
    {
        return findEvent(id);
    }

    [[nodiscard]] std::optional<Summary> findById(
        const Domain::CalendarEventId& id
        ) const
    {
        return findEvent(id);
    }

    [[nodiscard]] std::optional<Summary> lookupById(
        const Domain::CalendarEventId& id
        ) const
    {
        return findEvent(id);
    }

    [[nodiscard]] std::optional<Summary> find(
        const Domain::CalendarEventId& id
        ) const
    {
        return findEvent(id);
    }

    [[nodiscard]] std::optional<Summary> lookup(
        const Domain::CalendarEventId& id
        ) const
    {
        return findEvent(id);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_events.empty();
    }

    friend bool operator==(
        const CalendarEventProjection&,
        const CalendarEventProjection&
        ) = default;

private:
    explicit CalendarEventProjection(
        std::vector<Event> events
        )
        : m_events(std::move(events))
    {
    }

    std::vector<Event> m_events;
};

using CalendarEventSnapshot = CalendarEventProjection;
using CalendarEventListProjection = CalendarEventProjection;

} // namespace ClassMngr::Next::Application

#pragma once

#include "next/application/calendar_event_edit_draft.h"
#include "next/application/calendar_event_repeat_occurrence_plan.h"
#include "next/application/calendar_event_series_create_port.h"
#include "next/application/qt_compatible_text.h"
#include "next/domain/calendar_event_timing.h"
#include "next/domain/gregorian_date.h"

#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class CalendarEventEditValidationSeverity
{
    Warning,
    Error
};

using CalendarEventEditValidationArgument = std::variant<
    std::string,
    std::int64_t,
    std::vector<std::string>,
    std::vector<std::int64_t>,
    Domain::GregorianDate,
    Domain::CalendarEventClockTime
    >;

using CalendarEventEditValidationArguments =
    std::map<std::string, CalendarEventEditValidationArgument>;

struct CalendarEventEditValidationIssue final
{
    std::string code;
    std::string field;
    CalendarEventEditValidationSeverity severity =
        CalendarEventEditValidationSeverity::Error;
    CalendarEventEditValidationArguments arguments;

    friend bool operator==(
        const CalendarEventEditValidationIssue&,
        const CalendarEventEditValidationIssue&
        ) = default;
};

using CalendarEventEditValidationResult =
    std::vector<CalendarEventEditValidationIssue>;

struct CalendarEventRepeatValidationInput final
{
    CalendarEventRepeatFrequency frequency =
        CalendarEventRepeatFrequency::Weekly;
    std::string untilDate;
};

namespace CalendarEventEditValidationDetail
{

[[nodiscard]] inline std::uint32_t nextCodePoint(
    const std::string_view text,
    std::size_t& offset
    ) noexcept
{
    std::uint32_t codePoint = 0;
    if (QtCompatibleTextDetail::decodeNextUtf8CodePoint(
            text,
            offset,
            codePoint
            ))
    {
        return codePoint;
    }

    // The legacy UTF-8 boundary replaces malformed input. Treating each invalid
    // byte as one replacement character keeps title and identifier lengths
    // bounded and deterministic at this Qt-free boundary.
    ++offset;
    return 0xfffdU;
}

[[nodiscard]] inline bool isWhitespace(
    const std::uint32_t codePoint
    ) noexcept
{
    return codePoint <= 0xffffU
        && isQtWhitespace(static_cast<char16_t>(codePoint));
}

[[nodiscard]] inline std::size_t simplifiedUtf16Length(
    const std::string_view text
    ) noexcept
{
    std::size_t length = 0;
    std::size_t offset = 0;
    bool hasContent = false;
    bool pendingSpace = false;

    while (offset < text.size())
    {
        const std::uint32_t codePoint = nextCodePoint(text, offset);
        if (isWhitespace(codePoint))
        {
            pendingSpace = hasContent;
            continue;
        }

        if (pendingSpace)
        {
            ++length;
            pendingSpace = false;
        }

        length += codePoint > 0xffffU ? 2U : 1U;
        hasContent = true;
    }

    return length;
}

[[nodiscard]] inline std::size_t trimmedUtf16Length(
    const std::string_view text
    ) noexcept
{
    std::size_t length = 0;
    std::size_t trailingWhitespace = 0;
    std::size_t offset = 0;
    bool hasContent = false;

    while (offset < text.size())
    {
        const std::uint32_t codePoint = nextCodePoint(text, offset);
        const std::size_t units = codePoint > 0xffffU ? 2U : 1U;
        if (isWhitespace(codePoint))
        {
            if (hasContent)
            {
                trailingWhitespace += units;
            }
            continue;
        }

        length += trailingWhitespace + units;
        trailingWhitespace = 0;
        hasContent = true;
    }

    return length;
}

[[nodiscard]] inline std::string trimQtWhitespace(
    const std::string_view text
    )
{
    std::size_t offset = 0;
    std::size_t firstContent = std::string_view::npos;
    std::size_t lastContentEnd = 0;
    while (offset < text.size())
    {
        const std::size_t codePointStart = offset;
        const std::uint32_t codePoint = nextCodePoint(text, offset);
        if (!isWhitespace(codePoint))
        {
            if (firstContent == std::string_view::npos)
            {
                firstContent = codePointStart;
            }
            lastContentEnd = offset;
        }
    }

    if (firstContent == std::string_view::npos)
    {
        return {};
    }

    return std::string(text.substr(firstContent, lastContentEnd - firstContent));
}

[[nodiscard]] inline std::optional<Domain::CalendarEventClockTime>
clockTime(
    const std::optional<std::string>& value
    ) noexcept
{
    if (!value)
    {
        return std::nullopt;
    }
    return Domain::CalendarEventClockTime::fromCanonicalString(*value);
}

inline void addIssue(
    CalendarEventEditValidationResult& result,
    std::string code,
    std::string field,
    CalendarEventEditValidationArguments arguments = {}
    )
{
    result.push_back({
        .code = std::move(code),
        .field = std::move(field),
        .severity = CalendarEventEditValidationSeverity::Error,
        .arguments = std::move(arguments)
    });
}

[[nodiscard]] inline std::int64_t repeatFrequencyValue(
    const CalendarEventRepeatFrequency frequency
    ) noexcept
{
    return static_cast<std::int64_t>(frequency);
}

[[nodiscard]] inline std::size_t estimatedRepeatOccurrences(
    const Domain::GregorianDate startDate,
    const Domain::GregorianDate untilDate,
    const CalendarEventRepeatFrequency frequency
    ) noexcept
{
    using Domain::GregorianDate;

    if (untilDate.dayNumber() < startDate.dayNumber())
    {
        return 0;
    }

    std::size_t occurrences = 0;
    GregorianDate occurrenceDate = startDate;
    for (;;)
    {
        ++occurrences;
        if (occurrences > kCalendarEventSeriesCreateMaxOccurrences)
        {
            return occurrences;
        }

        const std::optional<GregorianDate> nextDate =
            CalendarEventRepeatOccurrencePlanDetail::nextOccurrenceDate(
                occurrenceDate,
                frequency
                );
        if (!nextDate || nextDate->dayNumber() > untilDate.dayNumber())
        {
            return occurrences;
        }
        occurrenceDate = *nextDate;
    }
}

} // namespace CalendarEventEditValidationDetail

// Returns diagnostics in the same field order consumed by the calendar
// dialog. Recurrence inputs are omitted when repetition is disabled.
[[nodiscard]] inline CalendarEventEditValidationResult
validateCalendarEventEditForm(
    const CalendarEventEditDraft& draft,
    const std::optional<CalendarEventRepeatValidationInput>& repetition =
        std::nullopt
    )
{
    using namespace CalendarEventEditValidationDetail;
    using Domain::CalendarEventClockTime;
    using Domain::GregorianDate;

    CalendarEventEditValidationResult result;
    const std::size_t titleLength = simplifiedUtf16Length(draft.title);
    if (titleLength == 0)
    {
        addIssue(result, "calendar.title.required", "title");
    }
    else if (titleLength > 255)
    {
        addIssue(
            result,
            "validation.length.out_of_bounds",
            "title",
            {
                {"length", static_cast<std::int64_t>(titleLength)},
                {"minimum", std::int64_t{1}},
                {"maximum", std::int64_t{255}}
            }
            );
    }

    const std::string eventType = trimQtWhitespace(draft.eventType);
    constexpr std::string_view eventTypes[] = {
        "Vacation", "Holiday", "Workshop", "CM", "Meeting", "Other"
    };
    bool validEventType = false;
    std::vector<std::string> allowedEventTypes;
    allowedEventTypes.reserve(std::size(eventTypes));
    for (const std::string_view allowedType : eventTypes)
    {
        allowedEventTypes.emplace_back(allowedType);
        validEventType = validEventType || eventType == allowedType;
    }
    if (!validEventType)
    {
        addIssue(
            result,
            "validation.enum.invalid_value",
            "eventType",
            {
                {"value", eventType},
                {"allowedValues", std::move(allowedEventTypes)}
            }
            );
    }

    const std::optional<std::string_view> timeStatusName =
        Domain::calendarEventTimeStatusName(draft.timeStatus);
    if (!timeStatusName)
    {
        addIssue(
            result,
            "validation.enum.invalid_value",
            "timeStatus",
            {
                {"value", std::string{}},
                {"allowedValues", std::vector<std::string>{
                     "Timed", "Unknown", "Unconfirmed"
                 }}
            }
            );
    }

    const std::string repeatSeriesId = draft.repeatSeriesId
        ? draft.repeatSeriesId->value()
        : std::string{};
    const std::size_t repeatSeriesIdLength =
        trimmedUtf16Length(repeatSeriesId);
    if (repeatSeriesIdLength > 128)
    {
        addIssue(
            result,
            "validation.length.out_of_bounds",
            "repeatSeriesId",
            {
                {"length", static_cast<std::int64_t>(repeatSeriesIdLength)},
                {"minimum", std::int64_t{0}},
                {"maximum", std::int64_t{128}}
            }
            );
    }

    const std::optional<GregorianDate> startDate =
        GregorianDate::fromIsoString(draft.startDate);
    const std::optional<GregorianDate> endDate =
        GregorianDate::fromIsoString(draft.endDate);
    if (!startDate)
    {
        addIssue(result, "calendar.date.invalid", "startDate");
    }
    if (!endDate)
    {
        addIssue(result, "calendar.date.invalid", "endDate");
    }
    if (startDate && endDate && endDate->dayNumber() < startDate->dayNumber())
    {
        addIssue(
            result,
            "calendar.date.end_before_start",
            "endDate",
            {
                {"startDate", *startDate},
                {"endDate", *endDate}
            }
            );
    }

    if (timeStatusName && draft.allDay)
    {
        if (draft.timeStatus != Domain::CalendarEventTimeStatus::Timed)
        {
            addIssue(
                result,
                "calendar.time_status.all_day_requires_timed",
                "timeStatus"
                );
        }
    }
    else if (timeStatusName
             && draft.timeStatus == Domain::CalendarEventTimeStatus::Timed)
    {
        const std::optional<CalendarEventClockTime> startTime =
            clockTime(draft.startTime);
        const std::optional<CalendarEventClockTime> endTime =
            clockTime(draft.endTime);
        if (!startTime)
        {
            addIssue(result, "calendar.time.invalid", "startTime");
        }
        if (!endTime)
        {
            addIssue(result, "calendar.time.invalid", "endTime");
        }

        const bool endNotAfterStart = startDate && endDate
            && startTime && endTime
            && (endDate->dayNumber() < startDate->dayNumber()
                || (endDate->dayNumber() == startDate->dayNumber()
                    && *endTime <= *startTime));
        if (endNotAfterStart)
        {
            addIssue(
                result,
                "calendar.time.end_not_after_start",
                "endTime",
                {
                    {"startDate", *startDate},
                    {"startTime", *startTime},
                    {"endDate", *endDate},
                    {"endTime", *endTime}
                }
                );
        }
    }
    else if (timeStatusName
             && (clockTime(draft.startTime) || clockTime(draft.endTime)))
    {
        addIssue(
            result,
            "calendar.time_status.requires_empty_times",
            "timeStatus",
            {{"timeStatus", std::string(*timeStatusName)}}
            );
    }

    if (!repetition)
    {
        return result;
    }

    if (!CalendarEventRepeatOccurrencePlanDetail::isSupportedFrequency(
            repetition->frequency
            ))
    {
        addIssue(
            result,
            "validation.enum.invalid_value",
            "repeat.frequency",
            {
                {"value", repeatFrequencyValue(repetition->frequency)},
                {"allowedValues", std::vector<std::int64_t>{0, 1, 2}}
            }
            );
    }

    const std::optional<GregorianDate> untilDate =
        GregorianDate::fromIsoString(repetition->untilDate);
    if (!untilDate)
    {
        addIssue(result, "calendar.repeat.until_date.invalid", "repeat.untilDate");
        return result;
    }

    if (!startDate)
    {
        return result;
    }

    if (untilDate->dayNumber() < startDate->dayNumber())
    {
        addIssue(
            result,
            "calendar.repeat.until_before_start",
            "repeat.untilDate",
            {
                {"startDate", *startDate},
                {"untilDate", *untilDate}
            }
            );
        return result;
    }

    const std::size_t occurrenceCount = estimatedRepeatOccurrences(
        *startDate,
        *untilDate,
        repetition->frequency
        );
    if (occurrenceCount > kCalendarEventSeriesCreateMaxOccurrences)
    {
        addIssue(
            result,
            "calendar.repeat.too_many_occurrences",
            "repeat.untilDate",
            {
                {"count", static_cast<std::int64_t>(occurrenceCount)},
                {"maximum", static_cast<std::int64_t>(
                     kCalendarEventSeriesCreateMaxOccurrences
                 )}
            }
            );
    }

    return result;
}

} // namespace ClassMngr::Next::Application

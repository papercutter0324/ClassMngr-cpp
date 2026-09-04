#include "classmngr/engine/calendar_event_service.h"

#include "classmngr/engine/calendar_event_rules.h"
#include "classmngr/engine/calendar_event_validator.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr int MinimumWireYear = 1;
constexpr int MaximumWireYear = 9999;
constexpr int MinutesPerDay = 24 * 60;

constexpr std::string_view EventColumns =
    "id, title, event_type, time_status, repeat_series_id, all_day, "
    "start_date, start_time, end_date, end_time";

Error error(
    ErrorCode code,
    std::string message
    )
{
    return {code, std::move(message), std::nullopt};
}

Error withContext(
    Error source,
    std::string_view action,
    std::string_view identity = {}
    )
{
    std::string message(action);
    if (!identity.empty())
    {
        message += " for ";
        message += identity;
    }
    message += " failed";
    if (!source.message.empty())
    {
        message += ": ";
        message += source.message;
    }
    source.message = std::move(message);
    return source;
}

bool isAsciiWhitespace(char character) noexcept
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size() && isAsciiWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isAsciiWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

bool isWireDate(const CalendarDate& date) noexcept
{
    const int year = static_cast<int>(date.year());
    return date.ok()
        && year >= MinimumWireYear
        && year <= MaximumWireYear;
}

Result<std::string> dateToWire(
    const CalendarDate& date
    )
{
    if (!isWireDate(date))
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Calendar event date is not representable as ISO YYYY-MM-DD."
            ));
    }

    const int year = static_cast<int>(date.year());
    const unsigned month = static_cast<unsigned>(date.month());
    const unsigned day = static_cast<unsigned>(date.day());

    std::string result = std::to_string(year);
    result.insert(0, 4U - result.size(), '0');
    result += '-';
    if (month < 10U)
    {
        result += '0';
    }
    result += std::to_string(month);
    result += '-';
    if (day < 10U)
    {
        result += '0';
    }
    result += std::to_string(day);
    return result;
}

Status validQueryDate(
    const CalendarDate& date,
    std::string_view action
    )
{
    if (isWireDate(date))
    {
        return {};
    }

    return std::unexpected(error(
        ErrorCode::InvalidArgument,
        std::string(action) + " requires a valid ISO calendar date."
        ));
}

Result<std::optional<CalendarDate>> nextRepeatDate(
    const CalendarDate& date,
    CalendarEventRepeatFrequency frequency
    )
{
    if (!isWireDate(date))
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Calendar repeat occurrence date is not representable as ISO YYYY-MM-DD."
            ));
    }

    switch (frequency)
    {
    case CalendarEventRepeatFrequency::Daily:
    case CalendarEventRepeatFrequency::Weekly:
    {
        const std::chrono::days increment =
            frequency == CalendarEventRepeatFrequency::Daily
                ? std::chrono::days{1}
                : std::chrono::days{7};
        const CalendarDate next{
            std::chrono::sys_days{date} + increment
        };
        return isWireDate(next)
            ? std::optional<CalendarDate>{next}
            : std::optional<CalendarDate>{};
    }

    case CalendarEventRepeatFrequency::Monthly:
    {
        const int currentYear = static_cast<int>(date.year());
        const unsigned currentMonth = static_cast<unsigned>(date.month());
        const unsigned currentDay = static_cast<unsigned>(date.day());
        const bool wrapsYear = currentMonth == 12U;
        const std::chrono::year nextYear{
            currentYear + (wrapsYear ? 1 : 0)
        };
        if (!nextYear.ok())
        {
            return std::optional<CalendarDate>{};
        }

        const std::chrono::month nextMonth{wrapsYear ? 1U : currentMonth + 1U};
        const auto lastDay = std::chrono::year_month_day_last{
            nextYear,
            std::chrono::month_day_last{nextMonth}
        }.day();
        const CalendarDate next{
            nextYear,
            nextMonth,
            std::chrono::day{std::min(
                currentDay,
                static_cast<unsigned>(lastDay)
                )}
        };
        return isWireDate(next)
            ? std::optional<CalendarDate>{next}
            : std::optional<CalendarDate>{};
    }
    }

    return std::unexpected(error(
        ErrorCode::InvalidArgument,
        "Calendar repeat frequency is not supported."
        ));
}

bool allDigits(std::string_view value) noexcept
{
    for (const char character : value)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
    }
    return true;
}

int twoDigitValue(std::string_view value) noexcept
{
    return (value[0] - '0') * 10 + (value[1] - '0');
}

Result<CalendarDate> dateFromValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-text calendar event "
                + std::string(column) + " value."
            ));
    }

    if (text->size() != 10
        || (*text)[4] != '-'
        || (*text)[7] != '-'
        || !allDigits(std::string_view(*text).substr(0, 4))
        || !allDigits(std::string_view(*text).substr(5, 2))
        || !allDigits(std::string_view(*text).substr(8, 2)))
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid calendar event "
                + std::string(column) + " date."
            ));
    }

    const int year =
        (twoDigitValue(std::string_view(*text).substr(0, 2)) * 100)
        + twoDigitValue(std::string_view(*text).substr(2, 2));
    const unsigned month = static_cast<unsigned>(twoDigitValue(
        std::string_view(*text).substr(5, 2)
        ));
    const unsigned day = static_cast<unsigned>(twoDigitValue(
        std::string_view(*text).substr(8, 2)
        ));
    const CalendarDate date{
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
    if (!isWireDate(date))
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid calendar event "
                + std::string(column) + " date."
            ));
    }

    return date;
}

Result<std::optional<std::chrono::minutes>> timeFromValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::nullopt;
    }

    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-text calendar event "
                + std::string(column) + " time."
            ));
    }
    if (text->empty())
    {
        return std::nullopt;
    }

    if (text->size() != 5
        || (*text)[2] != ':'
        || !allDigits(std::string_view(*text).substr(0, 2))
        || !allDigits(std::string_view(*text).substr(3, 2)))
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid calendar event "
                + std::string(column) + " time."
            ));
    }

    const int hour = twoDigitValue(std::string_view(*text).substr(0, 2));
    const int minute = twoDigitValue(std::string_view(*text).substr(3, 2));
    if (hour >= 24 || minute >= 60)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid calendar event "
                + std::string(column) + " time."
            ));
    }

    return std::chrono::minutes{hour * 60 + minute};
}

Result<std::string> requiredTextValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-text calendar event "
                + std::string(column) + " value."
            ));
    }
    return *text;
}

Result<std::string> nullableTextValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::string{};
    }

    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-text calendar event "
                + std::string(column) + " value."
            ));
    }
    return trimAsciiWhitespace(*text);
}

Result<int> positiveIntegerValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    const auto* integer = std::get_if<std::int64_t>(&value);
    if (integer == nullptr
        || *integer <= 0
        || *integer > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid calendar event "
                + std::string(column) + " value."
            ));
    }
    return static_cast<int>(*integer);
}

Result<bool> allDayValue(const SqliteValue& value)
{
    const auto* integer = std::get_if<std::int64_t>(&value);
    if (integer == nullptr || (*integer != 0 && *integer != 1))
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid calendar event all_day value."
            ));
    }
    return *integer != 0;
}

Result<CalendarEvent> eventFromRow(const SqliteRow& row)
{
    if (row.values.size() != 10)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected calendar event row shape."
            ));
    }

    const Result<int> id = positiveIntegerValue(row.values[0], "id");
    const Result<std::string> title = requiredTextValue(
        row.values[1],
        "title"
        );
    const Result<std::string> eventType = requiredTextValue(
        row.values[2],
        "event_type"
        );
    const Result<std::string> timeStatus = requiredTextValue(
        row.values[3],
        "time_status"
        );
    const Result<std::string> repeatSeriesId = nullableTextValue(
        row.values[4],
        "repeat_series_id"
        );
    const Result<bool> allDay = allDayValue(row.values[5]);
    const Result<CalendarDate> startDate = dateFromValue(
        row.values[6],
        "start_date"
        );
    const Result<std::optional<std::chrono::minutes>> startTime =
        timeFromValue(row.values[7], "start_time");
    const Result<CalendarDate> endDate = dateFromValue(
        row.values[8],
        "end_date"
        );
    const Result<std::optional<std::chrono::minutes>> endTime =
        timeFromValue(row.values[9], "end_time");

    if (!id)
    {
        return std::unexpected(id.error());
    }
    if (!title)
    {
        return std::unexpected(title.error());
    }
    if (!eventType)
    {
        return std::unexpected(eventType.error());
    }
    if (!timeStatus)
    {
        return std::unexpected(timeStatus.error());
    }
    if (!repeatSeriesId)
    {
        return std::unexpected(repeatSeriesId.error());
    }
    if (!allDay)
    {
        return std::unexpected(allDay.error());
    }
    if (!startDate)
    {
        return std::unexpected(startDate.error());
    }
    if (!startTime)
    {
        return std::unexpected(startTime.error());
    }
    if (!endDate)
    {
        return std::unexpected(endDate.error());
    }
    if (!endTime)
    {
        return std::unexpected(endTime.error());
    }

    CalendarEvent event;
    event.id = *id;
    event.title = *title;
    event.eventType = CalendarEventRules::normalizedEventType(*eventType);
    event.timeStatus = CalendarEventRules::normalizedTimeStatus(*timeStatus);
    event.repeatSeriesId = *repeatSeriesId;
    event.allDay = *allDay;
    event.startDate = *startDate;
    event.startTime = *startTime;
    event.endDate = *endDate;
    event.endTime = *endTime;
    return event;
}

std::string eventIdentity(const CalendarEvent& event)
{
    if (event.id > 0)
    {
        return "calendar event id " + std::to_string(event.id);
    }

    const Result<std::string> date = dateToWire(event.startDate);
    return "calendar event '"
        + event.title
        + "' on "
        + (date ? *date : std::string{"<invalid date>"});
}

std::string repeatSeriesIdentity(
    std::string_view repeatSeriesId,
    const CalendarDate& startDate
    )
{
    const Result<std::string> date = dateToWire(startDate);
    return "repeat series '"
        + std::string(repeatSeriesId)
        + "' from "
        + (date ? *date : std::string{"<invalid date>"});
}

std::string validationMessage(const ValidationResult& validation)
{
    std::string message = "Calendar event validation failed";
    bool first = true;
    for (const ValidationIssue& issue : validation.issues())
    {
        if (!issue.isError())
        {
            continue;
        }

        message += first ? ": " : "; ";
        first = false;
        message += issue.code;
        if (!issue.field.empty())
        {
            message += " (";
            message += issue.field;
            message += ')';
        }
    }
    message += '.';
    return message;
}

Result<std::string> timeToWire(
    const std::optional<std::chrono::minutes>& time,
    std::string_view column
    )
{
    if (!time.has_value())
    {
        return std::string{};
    }

    const auto count = time->count();
    if (count < 0 || count >= MinutesPerDay)
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Calendar event " + std::string(column)
                + " is outside the HH:mm range."
            ));
    }

    const auto hour = static_cast<int>(count / 60);
    const auto minute = static_cast<int>(count % 60);
    std::string result;
    result.reserve(5);
    if (hour < 10)
    {
        result += '0';
    }
    result += std::to_string(hour);
    result += ':';
    if (minute < 10)
    {
        result += '0';
    }
    result += std::to_string(minute);
    return result;
}

SqliteValue nullableText(std::string_view value)
{
    return value.empty()
        ? SqliteValue{std::monostate{}}
        : SqliteValue{std::string(value)};
}

Result<bool> rowExists(
    SqliteDatabase& database,
    int eventId
    )
{
    const auto rows = database.query(
        "SELECT EXISTS(SELECT 1 FROM calendar_events WHERE id=?)",
        SqliteParameters{SqliteValue{std::int64_t{eventId}}}
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.size() != 1 || rows->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected calendar event existence result."
            ));
    }

    const auto* value = std::get_if<std::int64_t>(
        &rows->rows.front().values.front()
        );
    if (value == nullptr || (*value != 0 && *value != 1))
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid calendar event existence result."
            ));
    }
    return *value != 0;
}

Result<int> insertedId(
    const SqliteQueryResult& rows
    )
{
    if (rows.rows.size() != 1 || rows.rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite did not return the new calendar event id."
            ));
    }
    return positiveIntegerValue(rows.rows.front().values.front(), "id");
}
} // namespace

CalendarEventService::CalendarEventService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<std::vector<CalendarEvent>> CalendarEventService::loadRows(
    std::string_view sql,
    const SqliteParameters& parameters,
    std::string_view action,
    std::string_view identity
    ) const
{
    const auto rows = m_database.query(sql, parameters);
    if (!rows)
    {
        return std::unexpected(withContext(rows.error(), action, identity));
    }

    std::vector<CalendarEvent> events;
    events.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<CalendarEvent> event = eventFromRow(row);
        if (!event)
        {
            return std::unexpected(withContext(
                event.error(),
                action,
                identity
                ));
        }
        events.push_back(*event);
    }
    return events;
}

Result<std::vector<CalendarEvent>> CalendarEventService::loadForDate(
    const CalendarDate& date
    )
{
    const Status valid = validQueryDate(date, "Loading calendar events for date");
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const Result<std::string> dateText = dateToWire(date);
    if (!dateText)
    {
        return std::unexpected(dateText.error());
    }

    return loadRows(
        "SELECT "
            + std::string(EventColumns)
            + " FROM calendar_events "
              "WHERE ? >= start_date AND ? <= end_date "
              "ORDER BY start_time, title",
        SqliteParameters{
            SqliteValue{*dateText},
            SqliteValue{*dateText}
        },
        "Loading calendar events for date",
        "date " + *dateText
        );
}

Result<std::vector<CalendarEvent>> CalendarEventService::loadInRange(
    const CalendarDate& startDate,
    const CalendarDate& endDate
    )
{
    const Status validStart = validQueryDate(
        startDate,
        "Loading calendar events in range"
        );
    if (!validStart)
    {
        return std::unexpected(validStart.error());
    }
    const Status validEnd = validQueryDate(
        endDate,
        "Loading calendar events in range"
        );
    if (!validEnd)
    {
        return std::unexpected(validEnd.error());
    }
    if (std::chrono::sys_days{endDate} < std::chrono::sys_days{startDate})
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Loading calendar events in range requires end date not before "
            "start date."
            ));
    }

    const Result<std::string> startText = dateToWire(startDate);
    const Result<std::string> endText = dateToWire(endDate);
    if (!startText)
    {
        return std::unexpected(startText.error());
    }
    if (!endText)
    {
        return std::unexpected(endText.error());
    }

    return loadRows(
        "SELECT "
            + std::string(EventColumns)
            + " FROM calendar_events "
              "WHERE end_date >= ? AND start_date <= ? "
              "ORDER BY start_date, start_time, title",
        SqliteParameters{
            SqliteValue{*startText},
            SqliteValue{*endText}
        },
        "Loading calendar events in range",
        "from " + *startText + " to " + *endText
        );
}

Result<std::vector<CalendarEvent>> CalendarEventService::loadUpcoming(
    const CalendarDate& fromDate,
    int limit
    )
{
    const Status valid = validQueryDate(
        fromDate,
        "Loading upcoming calendar events"
        );
    if (!valid)
    {
        return std::unexpected(valid.error());
    }
    if (limit <= 0)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Loading upcoming calendar events requires a positive limit."
            ));
    }

    const Result<std::string> dateText = dateToWire(fromDate);
    if (!dateText)
    {
        return std::unexpected(dateText.error());
    }

    return loadRows(
        "SELECT "
            + std::string(EventColumns)
            + " FROM calendar_events "
              "WHERE end_date >= ? "
              "ORDER BY start_date, start_time, title LIMIT ?",
        SqliteParameters{
            SqliteValue{*dateText},
            SqliteValue{std::int64_t{limit}}
        },
        "Loading upcoming calendar events",
        "from " + *dateText + ", limit " + std::to_string(limit)
        );
}

Result<std::optional<CalendarDate>> CalendarEventService::findNextStartDate(
    const CalendarDate& fromDate
    )
{
    const Status valid = validQueryDate(
        fromDate,
        "Finding next calendar event start date"
        );
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const Result<std::string> dateText = dateToWire(fromDate);
    if (!dateText)
    {
        return std::unexpected(dateText.error());
    }

    const auto rows = m_database.query(
        "SELECT MIN(start_date) FROM calendar_events WHERE start_date >= ?",
        SqliteParameters{SqliteValue{*dateText}}
        );
    if (!rows)
    {
        return std::unexpected(withContext(
            rows.error(),
            "Finding next calendar event start date",
            "from " + *dateText
            ));
    }
    if (rows->rows.size() != 1 || rows->rows.front().values.size() != 1)
    {
        return std::unexpected(withContext(
            error(
                ErrorCode::Schema,
                "SQLite returned an unexpected next calendar event date "
                "result."
                ),
            "Finding next calendar event start date",
            "from " + *dateText
            ));
    }

    const SqliteValue& value = rows->rows.front().values.front();
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::optional<CalendarDate>{};
    }

    const Result<CalendarDate> date = dateFromValue(value, "start_date");
    if (!date)
    {
        return std::unexpected(withContext(
            date.error(),
            "Finding next calendar event start date",
            "from " + *dateText
            ));
    }
    return std::optional<CalendarDate>{*date};
}

Result<CalendarEvent> CalendarEventService::get(
    int eventId
    )
{
    if (eventId <= 0)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Loading calendar event requires a positive event id."
            ));
    }

    const std::string identity = "calendar event id "
        + std::to_string(eventId);
    const auto rows = m_database.query(
        "SELECT "
            + std::string(EventColumns)
            + " FROM calendar_events WHERE id=?",
        SqliteParameters{SqliteValue{std::int64_t{eventId}}}
        );
    if (!rows)
    {
        return std::unexpected(withContext(
            rows.error(),
            "Loading calendar event",
            identity
            ));
    }
    if (rows->rows.empty())
    {
        return std::unexpected(withContext(
            error(
                ErrorCode::NotFound,
                "No calendar event exists for id "
                    + std::to_string(eventId) + "."
                ),
            "Loading calendar event",
            identity
            ));
    }
    if (rows->rows.size() != 1)
    {
        return std::unexpected(withContext(
            error(
                ErrorCode::Schema,
                "SQLite returned multiple calendar event rows for one id."
                ),
            "Loading calendar event",
            identity
            ));
    }

    const Result<CalendarEvent> event = eventFromRow(rows->rows.front());
    if (!event)
    {
        return std::unexpected(withContext(
            event.error(),
            "Loading calendar event",
            identity
            ));
    }
    return *event;
}

Result<std::vector<CalendarEvent>>
CalendarEventService::loadRepeatSeriesFromDate(
    std::string_view repeatSeriesId,
    const CalendarDate& startDate
    )
{
    const std::string normalizedRepeatSeriesId =
        trimAsciiWhitespace(repeatSeriesId);
    if (normalizedRepeatSeriesId.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Loading calendar repeat series requires a non-empty series id."
            ));
    }

    const Status valid = validQueryDate(
        startDate,
        "Loading calendar repeat series events"
        );
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const Result<std::string> dateText = dateToWire(startDate);
    if (!dateText)
    {
        return std::unexpected(dateText.error());
    }
    const std::string identity = repeatSeriesIdentity(
        normalizedRepeatSeriesId,
        startDate
        );

    return loadRows(
        "SELECT "
            + std::string(EventColumns)
            + " FROM calendar_events "
              "WHERE repeat_series_id=? AND start_date >= ? "
              "ORDER BY start_date, start_time, title, id",
        SqliteParameters{
            SqliteValue{normalizedRepeatSeriesId},
            SqliteValue{*dateText}
        },
        "Loading calendar repeat series events",
        identity
        );
}

Result<std::vector<CalendarEvent>>
CalendarEventService::expandRepeatSeries(
    const CalendarEvent& event,
    CalendarEventRepeatFrequency frequency,
    const CalendarDate& untilDate
    ) const
{
    const Result<CalendarEvent> normalized = normalizedForSave(event);
    if (!normalized)
    {
        return std::unexpected(withContext(
            normalized.error(),
            "Expanding calendar repeat series",
            eventIdentity(event)
            ));
    }

    if (normalized->repeatSeriesId.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Expanding a calendar repeat series requires a non-empty series id."
            ));
    }
    if (!isWireDate(untilDate))
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Calendar repeat until date is not representable as ISO YYYY-MM-DD."
            ));
    }

    const ValidationResult recurrenceValidation =
        CalendarEventValidator::validateRecurrence(
            *normalized,
            frequency,
            untilDate
            );
    if (recurrenceValidation.hasErrors())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            validationMessage(recurrenceValidation)
            ));
    }

    std::vector<CalendarEvent> occurrences;
    occurrences.reserve(static_cast<std::size_t>(
        CalendarEventValidator::estimatedRepeatOccurrences(
            normalized->startDate,
            untilDate,
            frequency
            )
        ));

    const std::chrono::days duration =
        std::chrono::sys_days{normalized->endDate}
        - std::chrono::sys_days{normalized->startDate};
    CalendarDate occurrenceDate = normalized->startDate;
    while (true)
    {
        CalendarEvent occurrence = *normalized;
        occurrence.id = -1;
        occurrence.startDate = occurrenceDate;
        occurrence.endDate = CalendarDate{
            std::chrono::sys_days{occurrenceDate} + duration
        };
        if (!isWireDate(occurrence.endDate))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Calendar repeat occurrence end date is not representable as ISO YYYY-MM-DD."
                ));
        }
        occurrences.push_back(std::move(occurrence));

        const Result<std::optional<CalendarDate>> next = nextRepeatDate(
            occurrenceDate,
            frequency
            );
        if (!next)
        {
            return std::unexpected(next.error());
        }
        if (!next->has_value()
            || std::chrono::sys_days{**next}
                > std::chrono::sys_days{untilDate})
        {
            break;
        }
        occurrenceDate = **next;
    }

    return occurrences;
}

Result<std::vector<int>> CalendarEventService::createRepeatSeries(
    const CalendarEvent& event,
    CalendarEventRepeatFrequency frequency,
    const CalendarDate& untilDate
    )
{
    const Result<std::vector<CalendarEvent>> occurrences = expandRepeatSeries(
        event,
        frequency,
        untilDate
        );
    if (!occurrences)
    {
        return std::unexpected(withContext(
            occurrences.error(),
            "Creating calendar repeat series",
            eventIdentity(event)
            ));
    }

    return saveBatch(*occurrences);
}

Status CalendarEventService::updateRepeatSeriesFromDate(
    const CalendarEvent& originalEvent,
    const CalendarEvent& editedEvent
    )
{
    const std::string repeatSeriesId = trimAsciiWhitespace(
        originalEvent.repeatSeriesId
        );
    if (repeatSeriesId.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Updating a calendar repeat series requires a non-empty original series id."
            ));
    }

    const Status validOriginalDate = validQueryDate(
        originalEvent.startDate,
        "Updating calendar repeat series events"
        );
    if (!validOriginalDate)
    {
        return validOriginalDate;
    }

    const Result<CalendarEvent> normalizedEdited = normalizedForSave(
        editedEvent
        );
    if (!normalizedEdited)
    {
        return std::unexpected(withContext(
            normalizedEdited.error(),
            "Updating calendar repeat series",
            eventIdentity(editedEvent)
            ));
    }

    const std::chrono::days startDateOffset =
        std::chrono::sys_days{normalizedEdited->startDate}
        - std::chrono::sys_days{originalEvent.startDate};
    const std::chrono::days duration =
        std::chrono::sys_days{normalizedEdited->endDate}
        - std::chrono::sys_days{normalizedEdited->startDate};

    auto transactionResult = m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting calendar repeat-series update transaction"
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Result<std::vector<CalendarEvent>> seriesEvents =
        loadRepeatSeriesFromDate(repeatSeriesId, originalEvent.startDate);
    if (!seriesEvents)
    {
        return std::unexpected(withContext(
            seriesEvents.error(),
            "Loading calendar repeat series for update",
            repeatSeriesIdentity(repeatSeriesId, originalEvent.startDate)
            ));
    }

    std::vector<CalendarEvent> updatedEvents;
    updatedEvents.reserve(seriesEvents->size());
    for (const CalendarEvent& seriesEvent : *seriesEvents)
    {
        CalendarEvent updatedEvent = seriesEvent;
        updatedEvent.title = normalizedEdited->title;
        updatedEvent.eventType = normalizedEdited->eventType;
        updatedEvent.timeStatus = normalizedEdited->timeStatus;
        updatedEvent.repeatSeriesId = repeatSeriesId;
        updatedEvent.allDay = normalizedEdited->allDay;
        updatedEvent.startTime = normalizedEdited->startTime;
        updatedEvent.endTime = normalizedEdited->endTime;
        updatedEvent.startDate = CalendarDate{
            std::chrono::sys_days{seriesEvent.startDate} + startDateOffset
        };
        updatedEvent.endDate = CalendarDate{
            std::chrono::sys_days{updatedEvent.startDate} + duration
        };
        if (!isWireDate(updatedEvent.startDate)
            || !isWireDate(updatedEvent.endDate))
        {
            return std::unexpected(error(
                ErrorCode::InvalidFormat,
                "Updated calendar repeat-series dates are not representable as ISO YYYY-MM-DD."
                ));
        }
        updatedEvents.push_back(std::move(updatedEvent));
    }

    const ValidationResult seriesValidation =
        CalendarEventValidator::validateSeries(updatedEvents);
    if (seriesValidation.hasErrors())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            validationMessage(seriesValidation)
            ));
    }

    for (const CalendarEvent& event : updatedEvents)
    {
        const Result<CalendarEvent> normalized = normalizedForSave(event);
        if (!normalized)
        {
            return std::unexpected(withContext(
                normalized.error(),
                "Updating calendar repeat series",
                eventIdentity(event)
                ));
        }
        const Result<int> saved = saveNormalized(*normalized);
        if (!saved)
        {
            return std::unexpected(withContext(
                saved.error(),
                "Updating calendar repeat series",
                eventIdentity(event)
                ));
        }
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing calendar repeat-series update"
            ));
    }
    return {};
}

Result<CalendarEvent> CalendarEventService::normalizedForSave(
    const CalendarEvent& event
    ) const
{
    CalendarEvent normalized = CalendarEventValidator::normalized(event);
    // The retained model has historically persisted all-day events as Timed
    // rows even when an older caller supplied another status.  Preserve that
    // storage contract while still validating every other field.
    if (normalized.allDay)
    {
        normalized.timeStatus = "Timed";
    }

    const ValidationResult validation = CalendarEventValidator::validate(
        normalized
        );
    if (validation.hasErrors())
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            validationMessage(validation)
            ));
    }

    const Result<std::string> startDate = dateToWire(normalized.startDate);
    const Result<std::string> endDate = dateToWire(normalized.endDate);
    if (!startDate)
    {
        return std::unexpected(startDate.error());
    }
    if (!endDate)
    {
        return std::unexpected(endDate.error());
    }

    return normalized;
}

Result<int> CalendarEventService::saveNormalized(
    const CalendarEvent& event
    )
{
    const std::string action = event.id > 0
        ? "Updating calendar event"
        : "Creating calendar event";
    const std::string identity = eventIdentity(event);

    const Result<std::string> startDate = dateToWire(event.startDate);
    const Result<std::string> endDate = dateToWire(event.endDate);
    const Result<std::string> startTime = timeToWire(
        event.startTime,
        "startTime"
        );
    const Result<std::string> endTime = timeToWire(
        event.endTime,
        "endTime"
        );
    if (!startDate)
    {
        return std::unexpected(withContext(
            startDate.error(),
            action,
            identity
            ));
    }
    if (!endDate)
    {
        return std::unexpected(withContext(endDate.error(), action, identity));
    }
    if (!startTime)
    {
        return std::unexpected(withContext(
            startTime.error(),
            action,
            identity
            ));
    }
    if (!endTime)
    {
        return std::unexpected(withContext(
            endTime.error(),
            action,
            identity
            ));
    }

    const SqliteParameters values{
        SqliteValue{event.title},
        SqliteValue{event.eventType},
        SqliteValue{event.timeStatus},
        nullableText(event.repeatSeriesId),
        SqliteValue{std::int64_t{event.allDay ? 1 : 0}},
        SqliteValue{*startDate},
        SqliteValue{*startTime},
        SqliteValue{*endDate},
        SqliteValue{*endTime}
    };

    if (event.id > 0)
    {
        const Result<bool> exists = rowExists(m_database, event.id);
        if (!exists)
        {
            return std::unexpected(withContext(
                exists.error(),
                action,
                identity
                ));
        }
        if (!*exists)
        {
            return std::unexpected(withContext(
                error(
                    ErrorCode::NotFound,
                    "No calendar event exists for id "
                        + std::to_string(event.id) + "."
                    ),
                action,
                identity
                ));
        }

        SqliteParameters updateValues = values;
        updateValues.push_back(SqliteValue{std::int64_t{event.id}});
        const Status updated = m_database.execute(
            "UPDATE calendar_events SET "
            "title=?, event_type=?, time_status=?, repeat_series_id=?, "
            "all_day=?, start_date=?, start_time=?, end_date=?, end_time=? "
            "WHERE id=?",
            updateValues
            );
        if (!updated)
        {
            return std::unexpected(withContext(
                updated.error(),
                action,
                identity
                ));
        }
        return event.id;
    }

    const Status inserted = m_database.execute(
        "INSERT INTO calendar_events ("
        "title, event_type, time_status, repeat_series_id, all_day, "
        "start_date, start_time, end_date, end_time"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
        values
        );
    if (!inserted)
    {
        return std::unexpected(withContext(
            inserted.error(),
            action,
            identity
            ));
    }

    const auto rowId = m_database.query("SELECT last_insert_rowid()");
    if (!rowId)
    {
        return std::unexpected(withContext(
            rowId.error(),
            action,
            identity
            ));
    }
    const Result<int> id = insertedId(*rowId);
    if (!id)
    {
        return std::unexpected(withContext(
            id.error(),
            action,
            identity
            ));
    }
    return *id;
}

Result<int> CalendarEventService::save(
    const CalendarEvent& event
    )
{
    const std::string action = event.id > 0
        ? "Updating calendar event"
        : "Creating calendar event";
    const Result<CalendarEvent> normalized = normalizedForSave(event);
    if (!normalized)
    {
        return std::unexpected(withContext(
            normalized.error(),
            action,
            eventIdentity(event)
            ));
    }
    return saveNormalized(*normalized);
}

Result<std::vector<int>> CalendarEventService::saveBatch(
    const std::vector<CalendarEvent>& events
    )
{
    if (events.empty())
    {
        return std::vector<int>{};
    }

    std::vector<CalendarEvent> normalizedEvents;
    normalizedEvents.reserve(events.size());
    for (const CalendarEvent& event : events)
    {
        const Result<CalendarEvent> normalized = normalizedForSave(event);
        if (!normalized)
        {
            return std::unexpected(withContext(
                normalized.error(),
                "Saving calendar event batch",
                eventIdentity(event)
                ));
        }
        normalizedEvents.push_back(*normalized);
    }

    const ValidationResult seriesValidation =
        CalendarEventValidator::validateSeries(normalizedEvents);
    if (seriesValidation.hasErrors())
    {
        return std::unexpected(withContext(
            error(
                ErrorCode::InvalidFormat,
                validationMessage(seriesValidation)
                ),
            "Saving calendar event batch"
            ));
    }

    auto transactionResult = m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting calendar event save transaction"
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    std::vector<int> eventIds;
    eventIds.reserve(normalizedEvents.size());
    for (const CalendarEvent& event : normalizedEvents)
    {
        const Result<int> saved = saveNormalized(event);
        if (!saved)
        {
            return std::unexpected(withContext(
                saved.error(),
                "Saving calendar event batch",
                eventIdentity(event)
                ));
        }
        eventIds.push_back(*saved);
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing calendar event saves"
            ));
    }
    return eventIds;
}

Result<CalendarEventImportSummary> CalendarEventService::importParsed(
    const CalendarImportResult& parsed
    )
{
    if (parsed.skippedCount < 0)
    {
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "The calendar import skipped count cannot be negative."
            ));
    }

    CalendarEventImportSummary summary;
    summary.skippedCount = parsed.skippedCount;
    if (parsed.events.empty())
    {
        return summary;
    }

    std::vector<CalendarEvent> normalizedEvents;
    normalizedEvents.reserve(parsed.events.size());
    for (const CalendarEvent& event : parsed.events)
    {
        const Result<CalendarEvent> normalized = normalizedForSave(event);
        if (!normalized)
        {
            return std::unexpected(withContext(
                normalized.error(),
                "Importing calendar events",
                eventIdentity(event)
                ));
        }
        normalizedEvents.push_back(*normalized);
    }

    const ValidationResult seriesValidation =
        CalendarEventValidator::validateSeries(normalizedEvents);
    if (seriesValidation.hasErrors())
    {
        return std::unexpected(withContext(
            error(
                ErrorCode::InvalidFormat,
                validationMessage(seriesValidation)
                ),
            "Importing calendar events"
            ));
    }

    CalendarDate firstDate = normalizedEvents.front().startDate;
    CalendarDate lastDate = normalizedEvents.front().endDate;
    for (const CalendarEvent& event : normalizedEvents)
    {
        if (std::chrono::sys_days{event.startDate}
            < std::chrono::sys_days{firstDate})
        {
            firstDate = event.startDate;
        }
        if (std::chrono::sys_days{event.endDate}
            > std::chrono::sys_days{lastDate})
        {
            lastDate = event.endDate;
        }
    }

    auto transactionResult = m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting calendar event import transaction"
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Result<std::vector<CalendarEvent>> existing = loadInRange(
        firstDate,
        lastDate
        );
    if (!existing)
    {
        return std::unexpected(withContext(
            existing.error(),
            "Loading existing calendar events for import"
            ));
    }

    std::set<std::string> signatures;
    for (const CalendarEvent& event : *existing)
    {
        signatures.insert(CalendarEventImportService::importSignature(event));
    }

    std::vector<CalendarEvent> eventsToSave;
    eventsToSave.reserve(normalizedEvents.size());
    for (const CalendarEvent& event : normalizedEvents)
    {
        const std::string signature =
            CalendarEventImportService::importSignature(event);
        if (!signatures.insert(signature).second)
        {
            ++summary.skippedCount;
            continue;
        }
        eventsToSave.push_back(event);
    }

    for (const CalendarEvent& event : eventsToSave)
    {
        const Result<int> saved = saveNormalized(event);
        if (!saved)
        {
            return std::unexpected(withContext(
                saved.error(),
                "Importing calendar events",
                eventIdentity(event)
                ));
        }
        ++summary.importedCount;
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing calendar event import"
            ));
    }
    return summary;
}

Status CalendarEventService::remove(
    int eventId
    )
{
    if (eventId <= 0)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Deleting calendar event requires a positive event id."
            ));
    }

    const std::string identity = "calendar event id "
        + std::to_string(eventId);
    const Status deleted = m_database.execute(
        "DELETE FROM calendar_events WHERE id=?",
        SqliteParameters{SqliteValue{std::int64_t{eventId}}}
        );
    if (!deleted)
    {
        return std::unexpected(withContext(
            deleted.error(),
            "Deleting calendar event",
            identity
            ));
    }
    return {};
}

Status CalendarEventService::removeRepeatSeriesFromDate(
    std::string_view repeatSeriesId,
    const CalendarDate& startDate
    )
{
    const std::string normalizedRepeatSeriesId =
        trimAsciiWhitespace(repeatSeriesId);
    if (normalizedRepeatSeriesId.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Deleting calendar repeat series requires a non-empty series id."
            ));
    }

    const Status valid = validQueryDate(
        startDate,
        "Deleting calendar repeat series events"
        );
    if (!valid)
    {
        return valid;
    }

    const Result<std::string> dateText = dateToWire(startDate);
    if (!dateText)
    {
        return std::unexpected(dateText.error());
    }
    const std::string identity = repeatSeriesIdentity(
        normalizedRepeatSeriesId,
        startDate
        );
    const Status deleted = m_database.execute(
        "DELETE FROM calendar_events "
        "WHERE repeat_series_id=? AND start_date >= ?",
        SqliteParameters{
            SqliteValue{normalizedRepeatSeriesId},
            SqliteValue{*dateText}
        }
        );
    if (!deleted)
    {
        return std::unexpected(withContext(
            deleted.error(),
            "Deleting calendar repeat series events",
            identity
            ));
    }
    return {};
}

Status CalendarEventService::removeAll()
{
    const Status deleted = m_database.execute(
        "DELETE FROM calendar_events"
        );
    if (!deleted)
    {
        return std::unexpected(withContext(
            deleted.error(),
            "Deleting all calendar events"
            ));
    }
    return {};
}

} // namespace classmngr::engine

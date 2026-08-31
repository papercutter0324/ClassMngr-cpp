#include "classmngr/engine/calendar_event_validator.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace classmngr::engine;

namespace
{
CalendarDate date(int year, unsigned month, unsigned day)
{
    return {
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
}

CalendarEvent validTimedEvent()
{
    CalendarEvent event;
    event.title = "Meeting";
    event.startDate = date(2026, 8, 31);
    event.startTime = std::chrono::minutes{9 * 60};
    event.endDate = date(2026, 8, 31);
    event.endTime = std::chrono::minutes{10 * 60};
    return event;
}

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineCalendarEventValidatorTests: "
              << message << '\n';
    return false;
}

bool hasIssue(
    const ValidationResult& result,
    std::string_view code,
    std::string_view field = {}
    )
{
    for (const ValidationIssue& issue : result.issues())
    {
        if (issue.code == code && (field.empty() || issue.field == field))
        {
            return true;
        }
    }

    return false;
}

bool hasIssueRow(
    const ValidationResult& result,
    std::string_view code,
    std::string_view field,
    int row
    )
{
    for (const ValidationIssue& issue : result.issues())
    {
        if (issue.code == code && issue.field == field && issue.row == row)
        {
            return true;
        }
    }

    return false;
}

int countIssues(
    const ValidationResult& result,
    std::string_view code
    )
{
    int count = 0;
    for (const ValidationIssue& issue : result.issues())
    {
        if (issue.code == code)
        {
            ++count;
        }
    }
    return count;
}
} // namespace

int main()
{
    bool passed = true;

    CalendarEvent source = validTimedEvent();
    source.title = "\t  New   Semester \n";
    source.eventType = "  Holiday\t";
    source.timeStatus = " Timed ";
    source.repeatSeriesId = " series-1 \r";
    const CalendarEvent normalized = CalendarEventValidator::normalized(source);
    passed &= expect(
        normalized.title == "New Semester"
            && normalized.eventType == "Holiday"
            && normalized.timeStatus == "Timed"
            && normalized.repeatSeriesId == "series-1",
        "calendar event strings were not normalized"
        );

    passed &= expect(
        CalendarEventValidator::validate(validTimedEvent()).isValid(),
        "valid timed event was rejected"
        );

    CalendarEvent imported = validTimedEvent();
    imported.timeStatus = "Unknown";
    imported.startTime.reset();
    imported.endTime.reset();
    passed &= expect(
        CalendarEventValidator::validate(imported).isValid(),
        "valid imported non-timed event was rejected"
        );

    CalendarEvent invalid = validTimedEvent();
    invalid.title = "   \t ";
    invalid.eventType = "holiday";
    invalid.timeStatus = "Timed";
    invalid.startDate = date(2026, 2, 30);
    invalid.endDate = date(2026, 2, 29);
    invalid.startTime = std::chrono::minutes{-1};
    invalid.endTime = std::chrono::minutes{1440};
    const ValidationResult invalidResult =
        CalendarEventValidator::validate(invalid);
    passed &= expect(
        hasIssue(invalidResult, "calendar.title.required", "title")
            && hasIssue(
                invalidResult,
                "validation.enum.invalid_value",
                "eventType"
                )
            && hasIssue(
                invalidResult,
                "calendar.date.invalid",
                "startDate"
                )
            && hasIssue(
                invalidResult,
                "calendar.date.invalid",
                "endDate"
                )
            && hasIssue(invalidResult, "calendar.time.invalid", "startTime")
            && hasIssue(invalidResult, "calendar.time.invalid", "endTime"),
        "invalid calendar event diagnostics changed"
        );

    CalendarEvent invalidStatus = validTimedEvent();
    invalidStatus.timeStatus = "unknown";
    passed &= expect(
        hasIssue(
            CalendarEventValidator::validate(invalidStatus),
            "validation.enum.invalid_value",
            "timeStatus"
            ),
        "invalid time status was not reported"
        );

    CalendarEvent reversed = validTimedEvent();
    reversed.endDate = date(2026, 8, 30);
    const ValidationResult reversedResult =
        CalendarEventValidator::validate(reversed);
    passed &= expect(
        hasIssue(
            reversedResult,
            "calendar.date.end_before_start",
            "endDate"
            )
            && hasIssue(
                reversedResult,
                "calendar.time.end_not_after_start",
                "endTime"
                ),
        "reversed date/time diagnostics changed"
        );

    CalendarEvent allDay = validTimedEvent();
    allDay.allDay = true;
    allDay.timeStatus = "Unknown";
    allDay.startTime.reset();
    allDay.endTime.reset();
    passed &= expect(
        hasIssue(
            CalendarEventValidator::validate(allDay),
            "calendar.time_status.all_day_requires_timed",
            "timeStatus"
            ),
        "all-day status rule changed"
        );

    CalendarEvent nonTimedWithTime = imported;
    nonTimedWithTime.endTime = std::chrono::minutes{1};
    passed &= expect(
        hasIssue(
            CalendarEventValidator::validate(nonTimedWithTime),
            "calendar.time_status.requires_empty_times",
            "timeStatus"
            ),
        "non-timed time rule changed"
        );

    CalendarEvent recurrenceEvent = validTimedEvent();
    recurrenceEvent.repeatSeriesId = "series-1";
    passed &= expect(
        CalendarEventValidator::validateRecurrence(
            recurrenceEvent,
            CalendarEventRepeatFrequency::Daily,
            date(2026, 9, 1)
            ).isValid()
            && CalendarEventValidator::estimatedRepeatOccurrences(
                date(2026, 8, 31),
                date(2026, 9, 1),
                CalendarEventRepeatFrequency::Daily
                ) == 2,
        "bounded daily recurrence changed"
        );
    passed &= expect(
        hasIssue(
            CalendarEventValidator::validateRecurrence(
                recurrenceEvent,
                CalendarEventRepeatFrequency::Daily,
                date(2026, 8, 30)
                ),
            "calendar.repeat.until_before_start",
            "repeat.untilDate"
            )
            && hasIssue(
                CalendarEventValidator::validateRecurrence(
                    recurrenceEvent,
                    CalendarEventRepeatFrequency::Daily,
                    date(2026, 2, 30)
                    ),
                "calendar.repeat.until_date.invalid",
                "repeat.untilDate"
                ),
        "recurrence date bounds changed"
        );
    passed &= expect(
        hasIssue(
            CalendarEventValidator::validateRecurrence(
                recurrenceEvent,
                CalendarEventRepeatFrequency::Daily,
                date(2028, 1, 1)
                ),
            "calendar.repeat.too_many_occurrences",
            "repeat.untilDate"
            ),
        "recurrence occurrence cap changed"
        );
    passed &= expect(
        hasIssue(
            CalendarEventValidator::validateRecurrence(
                recurrenceEvent,
                static_cast<CalendarEventRepeatFrequency>(99),
                date(2026, 9, 1)
                ),
            "validation.enum.invalid_value",
            "repeat.frequency"
            ),
        "invalid recurrence frequency was not reported"
        );

    std::vector<CalendarEvent> series(367, validTimedEvent());
    for (CalendarEvent& event : series)
    {
        event.repeatSeriesId = "series-1";
    }
    const ValidationResult seriesResult =
        CalendarEventValidator::validateSeries(series);
    passed &= expect(
        countIssues(
            seriesResult,
            "calendar.repeat.too_many_occurrences"
            ) == 367
            && hasIssue(
                seriesResult,
                "calendar.repeat.too_many_occurrences",
                "events[0].repeatSeriesId"
                )
            && hasIssue(
                seriesResult,
                "calendar.repeat.too_many_occurrences",
                "events[366].repeatSeriesId"
                )
            && hasIssueRow(
                seriesResult,
                "calendar.repeat.too_many_occurrences",
                "events[0].repeatSeriesId",
                0
                )
            && hasIssueRow(
                seriesResult,
                "calendar.repeat.too_many_occurrences",
                "events[366].repeatSeriesId",
                366
                ),
        "series occurrence cap changed"
        );

    CalendarEvent utf8 = validTimedEvent();
    utf8.title = "\xEC\x95\x88\xEB\x85\x95   \xE4\xB8\x96\xE7\x95\x8C";
    utf8.repeatSeriesId = "\xEC\x8B\x9C\xEB\xa6\xac";
    passed &= expect(
        CalendarEventValidator::normalized(utf8).title
                == "\xEC\x95\x88\xEB\x85\x95 \xE4\xB8\x96\xE7\x95\x8C"
            && CalendarEventValidator::validate(utf8).isValid(),
        "UTF-8 calendar event data was not preserved"
        );
    CalendarEvent oversized = validTimedEvent();
    oversized.title = std::string(256, 'x');
    passed &= expect(
        hasIssue(
            CalendarEventValidator::validate(oversized),
            "validation.length.out_of_bounds",
            "title"
            ),
        "title length limit changed"
        );

    CalendarEvent oversizedSeriesId = validTimedEvent();
    oversizedSeriesId.repeatSeriesId = std::string(129, 'x');
    passed &= expect(
        hasIssue(
            CalendarEventValidator::validate(oversizedSeriesId),
            "validation.length.out_of_bounds",
            "repeatSeriesId"
            ),
        "repeat-series length limit changed"
        );

    const CalendarDate january31 = date(2024, 1, 31);
    const CalendarDate may31 = date(2024, 5, 31);
    passed &= expect(
        CalendarEventValidator::estimatedRepeatOccurrences(
            january31,
            may31,
            CalendarEventRepeatFrequency::Monthly
            ) == 5,
        "month-end recurrence did not terminate deterministically"
        );
    passed &= expect(
        CalendarEventValidator::estimatedRepeatOccurrences(
            may31,
            january31,
            CalendarEventRepeatFrequency::Monthly
            ) == 0,
        "reversed recurrence range was not rejected"
        );

    return passed ? 0 : 1;
}

#include "classmngr/engine/calendar_event_service.h"
#include "classmngr/engine/open_database.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
namespace Engine = classmngr::engine;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineCalendarEventServiceTests: "
              << message
              << '\n';
    return false;
}

Engine::CalendarDate date(
    int year,
    unsigned month,
    unsigned day
    )
{
    return {
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
}

std::chrono::minutes time(
    int hour,
    int minute
    )
{
    return std::chrono::minutes{hour * 60 + minute};
}

Engine::CalendarEvent makeEvent(
    std::string_view title,
    const Engine::CalendarDate& startDate,
    const Engine::CalendarDate& endDate,
    std::chrono::minutes startTime,
    std::chrono::minutes endTime,
    std::string_view repeatSeriesId = {}
    )
{
    Engine::CalendarEvent event;
    event.title = title;
    event.eventType = "Meeting";
    event.timeStatus = "Timed";
    event.repeatSeriesId = repeatSeriesId;
    event.startDate = startDate;
    event.startTime = startTime;
    event.endDate = endDate;
    event.endTime = endTime;
    return event;
}

bool containsTitle(
    const std::vector<Engine::CalendarEvent>& events,
    std::string_view title
    )
{
    for (const Engine::CalendarEvent& event : events)
    {
        if (event.title == title)
        {
            return true;
        }
    }
    return false;
}

bool containsOccurrence(
    const std::vector<Engine::CalendarEvent>& events,
    std::string_view title,
    const Engine::CalendarDate& startDate,
    const Engine::CalendarDate& endDate
    )
{
    for (const Engine::CalendarEvent& event : events)
    {
        if (event.title == title
            && event.startDate == startDate
            && event.endDate == endDate)
        {
            return true;
        }
    }
    return false;
}
} // namespace

int main()
{
    const auto opened = Engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineCalendarEventServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    Engine::CalendarEventService service(database);
    const Engine::CalendarDate july10 = date(2026, 7, 10);
    const Engine::CalendarDate july12 = date(2026, 7, 12);
    const Engine::CalendarDate july20 = date(2026, 7, 20);
    bool passed = true;

    Engine::CalendarEvent first = makeEvent(
        "  UTF-8  Event / 행사  ",
        july10,
        july12,
        time(9, 0),
        time(10, 0),
        " series-1 "
        );
    first.eventType = " Meeting ";

    const auto created = service.save(first);
    passed &= expect(
        created && *created > 0,
        "valid calendar event creation failed"
        );
    if (!created)
    {
        return 1;
    }

    const auto loaded = service.get(*created);
    passed &= expect(
        loaded
            && loaded->id == *created
            && loaded->title == "UTF-8 Event / 행사"
            && loaded->eventType == "Meeting"
            && loaded->repeatSeriesId == "series-1"
            && loaded->startTime == time(9, 0)
            && loaded->endTime == time(10, 0),
        "calendar event UTF-8, normalization, or time round trip failed"
        );

    Engine::CalendarEvent allDay = makeEvent(
        "All day",
        date(2026, 7, 15),
        date(2026, 7, 15),
        time(0, 0),
        time(1, 0)
        );
    allDay.timeStatus = "Unknown";
    allDay.allDay = true;
    allDay.startTime.reset();
    allDay.endTime.reset();
    const auto allDayId = service.save(allDay);
    const auto loadedAllDay = allDayId
        ? service.get(*allDayId)
        : Engine::Result<Engine::CalendarEvent>{
            std::unexpected(Engine::Error{
                Engine::ErrorCode::Internal,
                "all-day save did not return an id",
                std::nullopt
            })
        };
    passed &= expect(
        allDayId
            && loadedAllDay
            && loadedAllDay->allDay
            && loadedAllDay->timeStatus == "Timed"
            && !loadedAllDay->startTime
            && !loadedAllDay->endTime,
        "all-day calendar event did not preserve the storage contract"
        );

    Engine::CalendarEvent future = makeEvent(
        "Future",
        july20,
        july20,
        time(8, 0),
        time(9, 0),
        "series-1"
        );
    const auto futureId = service.save(future);
    passed &= expect(futureId && *futureId > 0, "future event save failed");

    const auto forDate = service.loadForDate(date(2026, 7, 11));
    passed &= expect(
        forDate && forDate->size() == 1
            && forDate->front().title == "UTF-8 Event / 행사",
        "date query did not return the spanning event"
        );

    const auto range = service.loadInRange(
        date(2026, 7, 13),
        date(2026, 7, 21)
        );
    passed &= expect(
        range && range->size() == 2
            && containsTitle(*range, "All day")
            && containsTitle(*range, "Future"),
        "range query did not preserve overlap semantics"
        );

    const auto upcoming = service.loadUpcoming(date(2026, 7, 15), 1);
    passed &= expect(
        upcoming && upcoming->size() == 1
            && upcoming->front().title == "All day",
        "upcoming query did not apply the date and limit semantics"
        );

    const auto next = service.findNextStartDate(date(2026, 7, 16));
    passed &= expect(
        next && next->has_value() && **next == july20,
        "next event query did not find the earliest future start date"
        );
    const auto noNext = service.findNextStartDate(date(2026, 9, 1));
    passed &= expect(
        noNext && !noNext->has_value(),
        "next event query did not return an empty date when no event exists"
        );

    const auto series = service.loadRepeatSeriesFromDate(
        " series-1 ",
        date(2026, 7, 11)
        );
    passed &= expect(
        series && series->size() == 1 && series->front().title == "Future",
        "repeat-series query did not select the requested suffix"
        );

    if (loaded)
    {
        Engine::CalendarEvent updated = *loaded;
        updated.title = "Updated";
        const auto saved = service.save(updated);
        const auto reloaded = saved ? service.get(*saved)
                                    : Engine::Result<Engine::CalendarEvent>{
            std::unexpected(Engine::Error{
                Engine::ErrorCode::Internal,
                "update did not return an id",
                std::nullopt
            })
        };
        passed &= expect(
            saved && reloaded && reloaded->title == "Updated",
            "calendar event update failed"
            );
    }

    Engine::CalendarEvent invalid = future;
    invalid.eventType = "Unsupported";
    const auto rejected = service.save(invalid);
    passed &= expect(
        !rejected && rejected.error().code == Engine::ErrorCode::InvalidFormat,
        "unsupported event type was not rejected at the service boundary"
        );

    passed &= expect(
        !service.get(0)
            && service.get(0).error().code == Engine::ErrorCode::InvalidArgument,
        "invalid event id was not rejected"
        );

    passed &= expect(
        database.execute(
            "CREATE TRIGGER reject_calendar_insert "
            "BEFORE INSERT ON calendar_events "
            "WHEN NEW.title = 'Reject' BEGIN "
            "SELECT RAISE(ABORT, 'injected calendar insert failure'); END"
            ).has_value(),
        "calendar insert failure trigger could not be created"
        );
    Engine::CalendarEvent acceptedBeforeReject = makeEvent(
        "Before reject",
        date(2026, 8, 1),
        date(2026, 8, 1),
        time(9, 0),
        time(10, 0)
        );
    Engine::CalendarEvent rejectedEvent = acceptedBeforeReject;
    rejectedEvent.title = "Reject";
    const auto batch = service.saveBatch({acceptedBeforeReject, rejectedEvent});
    passed &= expect(
        !batch,
        "calendar batch did not report the injected write failure"
        );
    const auto afterBatch = service.loadInRange(
        date(2026, 8, 1),
        date(2026, 8, 1)
        );
    passed &= expect(
        afterBatch && afterBatch->empty(),
        "calendar batch failure was not rolled back"
        );
    passed &= expect(
        database.execute("DROP TRIGGER reject_calendar_insert").has_value(),
        "calendar insert failure trigger could not be removed"
        );

    Engine::CalendarEvent recurrence = makeEvent(
        "Repeat",
        date(2026, 1, 30),
        date(2026, 1, 31),
        time(9, 0),
        time(10, 0),
        "engine-repeat"
        );
    const auto dailyOccurrences = service.expandRepeatSeries(
        recurrence,
        Engine::CalendarEventRepeatFrequency::Daily,
        date(2026, 2, 1)
        );
    passed &= expect(
        dailyOccurrences && dailyOccurrences->size() == 3
            && dailyOccurrences->front().id == -1
            && containsOccurrence(
                *dailyOccurrences,
                "Repeat",
                date(2026, 2, 1),
                date(2026, 2, 2)
                ),
        "daily repeat expansion did not preserve unsaved ids and duration"
        );

    const auto weeklyOccurrences = service.expandRepeatSeries(
        recurrence,
        Engine::CalendarEventRepeatFrequency::Weekly,
        date(2026, 2, 13)
        );
    passed &= expect(
        weeklyOccurrences && weeklyOccurrences->size() == 3
            && containsOccurrence(
                *weeklyOccurrences,
                "Repeat",
                date(2026, 2, 13),
                date(2026, 2, 14)
                ),
        "weekly repeat expansion did not preserve seven-day spacing"
        );

    Engine::CalendarEvent monthEnd = makeEvent(
        "Month end",
        date(2026, 1, 31),
        date(2026, 2, 1),
        time(9, 0),
        time(10, 0),
        "engine-month-end"
        );
    const auto monthlyOccurrences = service.expandRepeatSeries(
        monthEnd,
        Engine::CalendarEventRepeatFrequency::Monthly,
        date(2026, 4, 30)
        );
    passed &= expect(
        monthlyOccurrences && monthlyOccurrences->size() == 4
            && containsOccurrence(
                *monthlyOccurrences,
                "Month end",
                date(2026, 2, 28),
                date(2026, 3, 1)
                )
            && containsOccurrence(
                *monthlyOccurrences,
                "Month end",
                date(2026, 4, 28),
                date(2026, 4, 29)
                ),
        "monthly repeat expansion did not retain the established month-end clamp"
        );

    const auto invalidUntil = service.expandRepeatSeries(
        recurrence,
        Engine::CalendarEventRepeatFrequency::Daily,
        date(2026, 1, 29)
        );
    Engine::CalendarEvent missingSeriesId = recurrence;
    missingSeriesId.repeatSeriesId.clear();
    const auto invalidSeriesId = service.expandRepeatSeries(
        missingSeriesId,
        Engine::CalendarEventRepeatFrequency::Daily,
        date(2026, 2, 1)
        );
    passed &= expect(
        !invalidUntil
            && invalidUntil.error().code == Engine::ErrorCode::InvalidFormat
            && !invalidSeriesId
            && invalidSeriesId.error().code == Engine::ErrorCode::InvalidArgument,
        "repeat expansion did not reject invalid bounds or a missing series id"
        );

    Engine::CalendarEvent seriesSeed = makeEvent(
        "Series base",
        date(2026, 3, 1),
        date(2026, 3, 2),
        time(9, 0),
        time(10, 0),
        "engine-update-series"
        );
    const auto createdSeries = service.createRepeatSeries(
        seriesSeed,
        Engine::CalendarEventRepeatFrequency::Daily,
        date(2026, 3, 3)
        );
    const auto originalSeries = service.loadRepeatSeriesFromDate(
        "engine-update-series",
        date(2026, 3, 1)
        );
    passed &= expect(
        createdSeries && createdSeries->size() == 3
            && originalSeries && originalSeries->size() == 3,
        "repeat-series creation did not persist all occurrences"
        );

    if (originalSeries && originalSeries->size() == 3)
    {
        Engine::CalendarEvent edited = makeEvent(
            "Series edited",
            date(2026, 3, 3),
            date(2026, 3, 5),
            time(11, 0),
            time(12, 0),
            "ignored-edited-series-id"
            );
        const auto updated = service.updateRepeatSeriesFromDate(
            originalSeries->at(1),
            edited
            );
        const auto reloadedSeries = service.loadRepeatSeriesFromDate(
            "engine-update-series",
            date(2026, 3, 1)
            );
        passed &= expect(
            updated && reloadedSeries && reloadedSeries->size() == 3
                && reloadedSeries->at(0).id == originalSeries->at(0).id
                && reloadedSeries->at(1).id == originalSeries->at(1).id
                && reloadedSeries->at(2).id == originalSeries->at(2).id
                && containsOccurrence(
                    *reloadedSeries,
                    "Series base",
                    date(2026, 3, 1),
                    date(2026, 3, 2)
                    )
                && containsOccurrence(
                    *reloadedSeries,
                    "Series edited",
                    date(2026, 3, 3),
                    date(2026, 3, 5)
                    )
                && containsOccurrence(
                    *reloadedSeries,
                    "Series edited",
                    date(2026, 3, 4),
                    date(2026, 3, 6)
                    )
                && reloadedSeries->at(1).startTime == time(11, 0)
                && reloadedSeries->at(1).repeatSeriesId
                    == "engine-update-series",
            "repeat-series update did not preserve suffix, ids, duration, and identity"
            );
    }

    Engine::CalendarEvent rollbackSeed = makeEvent(
        "Rollback base",
        date(2026, 4, 10),
        date(2026, 4, 11),
        time(9, 0),
        time(10, 0),
        "engine-rollback-series"
        );
    const auto rollbackCreated = service.createRepeatSeries(
        rollbackSeed,
        Engine::CalendarEventRepeatFrequency::Daily,
        date(2026, 4, 12)
        );
    const auto rollbackOriginal = service.loadRepeatSeriesFromDate(
        "engine-rollback-series",
        date(2026, 4, 10)
        );
    passed &= expect(
        rollbackCreated && rollbackOriginal && rollbackOriginal->size() == 3
            && database.execute(
                "CREATE TRIGGER reject_calendar_repeat_update "
                "BEFORE UPDATE ON calendar_events "
                "WHEN NEW.title = 'Rollback reject' BEGIN "
                "SELECT RAISE(ABORT, 'injected calendar repeat update failure'); END"
                ).has_value(),
        "repeat-series update rollback fixture could not be created"
        );
    if (rollbackOriginal && rollbackOriginal->size() == 3)
    {
        Engine::CalendarEvent rejectedEdit = makeEvent(
            "Rollback reject",
            date(2026, 4, 11),
            date(2026, 4, 13),
            time(11, 0),
            time(12, 0)
            );
        const auto rejectedUpdate = service.updateRepeatSeriesFromDate(
            rollbackOriginal->front(),
            rejectedEdit
            );
        const auto afterRejectedUpdate = service.loadRepeatSeriesFromDate(
            "engine-rollback-series",
            date(2026, 4, 10)
            );
        passed &= expect(
            !rejectedUpdate && afterRejectedUpdate
                && afterRejectedUpdate->size() == 3
                && containsOccurrence(
                    *afterRejectedUpdate,
                    "Rollback base",
                    date(2026, 4, 10),
                    date(2026, 4, 11)
                    )
                && containsOccurrence(
                    *afterRejectedUpdate,
                    "Rollback base",
                    date(2026, 4, 12),
                    date(2026, 4, 13)
                    ),
            "repeat-series update failure was not rolled back"
            );
    }
    passed &= expect(
        database.execute("DROP TRIGGER reject_calendar_repeat_update").has_value(),
        "repeat-series update rollback fixture could not be removed"
        );

    if (futureId)
    {
        passed &= expect(
            service.removeRepeatSeriesFromDate("series-1", july20).has_value(),
            "repeat-series deletion failed"
            );
        const auto removedFuture = service.get(*futureId);
        passed &= expect(
            !removedFuture && removedFuture.error().code
                == Engine::ErrorCode::NotFound,
            "repeat-series deletion did not remove the selected suffix"
            );
    }

    passed &= expect(
        service.removeAll().has_value()
            && service.loadInRange(date(2026, 1, 1), date(2026, 12, 31))
                   ->empty(),
        "calendar remove-all did not clear persisted events"
        );

    return passed ? 0 : 1;
}

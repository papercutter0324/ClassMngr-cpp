#pragma once

#include "classmngr/engine/calendar_event.h"
#include "classmngr/engine/calendar_event_import_service.h"
#include "classmngr/engine/result.h"
#include "classmngr/engine/sqlite_database.h"

#include <optional>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

// Calendar-event persistence shared by native and retained desktop adapters.
// The service owns the SQLite workflow; presentation layers only translate
// their models at this boundary.
class CalendarEventService final
{
public:
    explicit CalendarEventService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<std::vector<CalendarEvent>> loadForDate(
        const CalendarDate& date
        );

    [[nodiscard]] Result<std::vector<CalendarEvent>> loadInRange(
        const CalendarDate& startDate,
        const CalendarDate& endDate
        );

    [[nodiscard]] Result<std::vector<CalendarEvent>> loadUpcoming(
        const CalendarDate& fromDate,
        int limit
        );

    [[nodiscard]] Result<std::optional<CalendarDate>> findNextStartDate(
        const CalendarDate& fromDate
        );

    [[nodiscard]] Result<CalendarEvent> get(
        int eventId
        );

    [[nodiscard]] Result<std::vector<CalendarEvent>> loadRepeatSeriesFromDate(
        std::string_view repeatSeriesId,
        const CalendarDate& startDate
        );

    // Expands a caller-owned repeat-series id into unsaved occurrences.  Each
    // returned event has id=-1; callers retain ownership of generating the
    // repeatSeriesId before invoking this operation.
    [[nodiscard]] Result<std::vector<CalendarEvent>> expandRepeatSeries(
        const CalendarEvent& event,
        CalendarEventRepeatFrequency frequency,
        const CalendarDate& untilDate
        ) const;

    // Atomically expands and persists a repeat series.  event.repeatSeriesId
    // must be a non-empty caller-owned identifier.
    [[nodiscard]] Result<std::vector<int>> createRepeatSeries(
        const CalendarEvent& event,
        CalendarEventRepeatFrequency frequency,
        const CalendarDate& untilDate
        );

    // Atomically applies editedEvent's editable fields to the persisted
    // suffix of originalEvent's repeat series.  The original series id is
    // retained and the edited start-date offset and duration are propagated
    // to every affected occurrence.
    [[nodiscard]] Status updateRepeatSeriesFromDate(
        const CalendarEvent& originalEvent,
        const CalendarEvent& editedEvent
        );

    [[nodiscard]] Result<int> save(
        const CalendarEvent& event
        );

    [[nodiscard]] Result<std::vector<int>> saveBatch(
        const std::vector<CalendarEvent>& events
        );

    // Imports parser-owned calendar content as one atomic operation.  The
    // service owns duplicate detection against persisted events and batch
    // persistence; callers only present parsed import content.
    [[nodiscard]] Result<CalendarEventImportSummary> importParsed(
        const CalendarImportResult& parsed
        );

    [[nodiscard]] Status remove(
        int eventId
        );

    [[nodiscard]] Status removeRepeatSeriesFromDate(
        std::string_view repeatSeriesId,
        const CalendarDate& startDate
        );

    [[nodiscard]] Status removeAll();

private:
    [[nodiscard]] Result<CalendarEvent> normalizedForSave(
        const CalendarEvent& event
        ) const;

    [[nodiscard]] Result<int> saveNormalized(
        const CalendarEvent& event
        );

    [[nodiscard]] Result<std::vector<CalendarEvent>> loadRows(
        std::string_view sql,
        const SqliteParameters& parameters,
        std::string_view action,
        std::string_view identity
        ) const;

    SqliteDatabase& m_database;
};

} // namespace classmngr::engine

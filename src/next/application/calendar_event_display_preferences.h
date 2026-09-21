#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// Typed calendar event-display preferences. Persistence and legacy QVariant
// conversion remain outside the application layer.
struct CalendarEventDisplayPreferences final
{
    bool showEventsAtAllCampuses = false;
    bool hideStartOfTermEvents = false;

    friend bool operator==(
        const CalendarEventDisplayPreferences&,
        const CalendarEventDisplayPreferences&
        ) = default;
};

using CalendarEventDisplayPreferencesResult =
    Domain::Result<CalendarEventDisplayPreferences>;
using CalendarEventDisplayPreferencesError = Domain::OperationError;
using CalendarEventDisplayPreferencesRequest =
    CalendarEventDisplayPreferences;
using CalendarEventDisplayPreferencesSaveRequest =
    CalendarEventDisplayPreferencesRequest;
using CalendarEventDisplayPreferencesSaveResult = Domain::Result<void>;
using CalendarEventDisplayPreferencesSaveError = Domain::OperationError;

// The application boundary exposes typed load and atomic save operations;
// persistence and presentation conversion belong to an outer adapter.
class CalendarEventDisplayPreferencesPort
{
public:
    virtual ~CalendarEventDisplayPreferencesPort() = default;

    [[nodiscard]] virtual CalendarEventDisplayPreferencesResult load()
        const = 0;

    [[nodiscard]] virtual CalendarEventDisplayPreferencesSaveResult save(
        const CalendarEventDisplayPreferencesSaveRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

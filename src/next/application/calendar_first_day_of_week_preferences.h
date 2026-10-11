#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries the typed first day of week. Persistence
// and legacy storage conversion belong to the platform adapter.
enum class CalendarFirstDayOfWeek
{
    Sunday = 0,
    Monday = 1,
    Tuesday = 2,
    Wednesday = 3,
    Thursday = 4,
    Friday = 5,
    Saturday = 6
};

class CalendarFirstDayOfWeekPreferencesPort
{
public:
    virtual ~CalendarFirstDayOfWeekPreferencesPort() = default;

    [[nodiscard]] virtual CalendarFirstDayOfWeek load() const = 0;

    using SaveResult = Domain::Result<void>;

    [[nodiscard]] virtual SaveResult save(
        CalendarFirstDayOfWeek firstDayOfWeek
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

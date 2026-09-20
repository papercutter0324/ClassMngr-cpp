#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The calendar display boundary currently needs only this bounded, typed
// read. Persistence and legacy QVariant conversion remain outside the
// application layer.
struct ScheduleDisplayPreferences final
{
    bool use24HourTime = false;

    friend bool operator==(
        const ScheduleDisplayPreferences&,
        const ScheduleDisplayPreferences&
        ) = default;
};

using ScheduleDisplayPreferencesResult =
    Domain::Result<ScheduleDisplayPreferences>;
using ScheduleDisplayPreferencesError = Domain::OperationError;

// Read-only schedule display preferences. The application boundary exposes
// no persistence or mutation operation for this legacy setting.
class ScheduleDisplayPreferencesPort
{
public:
    virtual ~ScheduleDisplayPreferencesPort() = default;

    [[nodiscard]] virtual ScheduleDisplayPreferencesResult load() const = 0;
};

} // namespace ClassMngr::Next::Application

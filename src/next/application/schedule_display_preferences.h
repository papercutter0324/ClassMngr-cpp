#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// Typed schedule display preferences. Persistence and legacy QVariant
// conversion remain outside the application layer.
struct ScheduleDisplayPreferences final
{
    bool use24HourTime = false;
    bool showEnglishNames = false;
    bool showWeekends = false;
    bool showAllIntensiveHours = false;
    bool testingAffectsM1 = false;

    friend bool operator==(
        const ScheduleDisplayPreferences&,
        const ScheduleDisplayPreferences&
        ) = default;
};

using ScheduleDisplayPreferencesResult =
    Domain::Result<ScheduleDisplayPreferences>;
using ScheduleDisplayPreferencesError = Domain::OperationError;
using ScheduleDisplayPreferencesRequest = ScheduleDisplayPreferences;
using ScheduleDisplayPreferencesSaveRequest = ScheduleDisplayPreferencesRequest;
using ScheduleDisplayPreferencesSaveResult = Domain::Result<void>;
using ScheduleDisplayPreferencesSaveError = Domain::OperationError;

// The application boundary exposes typed load and atomic save operations;
// persistence and presentation conversion belong to an outer adapter.
class ScheduleDisplayPreferencesPort
{
public:
    virtual ~ScheduleDisplayPreferencesPort() = default;

    [[nodiscard]] virtual ScheduleDisplayPreferencesResult load() const = 0;

    [[nodiscard]] virtual ScheduleDisplayPreferencesSaveResult save(
        const ScheduleDisplayPreferencesSaveRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application

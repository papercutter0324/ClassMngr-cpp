#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed schedule display mode.
// Persistence and legacy string conversion belong to the outer adapter.
enum class ScheduleDisplayMode
{
    Regular,
    Intensive,
    Testing
};

using ScheduleDisplayModePreferencesSaveResult = Domain::Result<void>;
using ScheduleDisplayModePreferencesSaveError = Domain::OperationError;
using ScheduleDisplayModePreferencesLoadResult =
    Domain::Result<ScheduleDisplayMode>;
using ScheduleDisplayModePreferencesLoadError = Domain::OperationError;

class ScheduleDisplayModePreferencesPort
{
public:
    virtual ~ScheduleDisplayModePreferencesPort() = default;

    [[nodiscard]] virtual ScheduleDisplayModePreferencesLoadResult load()
        const = 0;

    [[nodiscard]] virtual ScheduleDisplayModePreferencesSaveResult save(
        ScheduleDisplayMode mode
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

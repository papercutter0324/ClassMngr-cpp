#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// Typed sidebar display preferences. Persistence and legacy QVariant
// conversion remain outside the application layer.
struct SidebarDisplayPreferences final
{
    bool sidebarTooltipsEnabled = true;
    bool sidebarMarqueeEnabled = true;

    friend bool operator==(
        const SidebarDisplayPreferences&,
        const SidebarDisplayPreferences&
        ) = default;
};

using SidebarDisplayPreferencesResult =
    Domain::Result<SidebarDisplayPreferences>;
using SidebarDisplayPreferencesError = Domain::OperationError;
using SidebarDisplayPreferencesRequest = SidebarDisplayPreferences;
using SidebarDisplayPreferencesSaveRequest =
    SidebarDisplayPreferencesRequest;
using SidebarDisplayPreferencesSaveResult = Domain::Result<void>;
using SidebarDisplayPreferencesSaveError = Domain::OperationError;

// The application boundary exposes typed load and save operations;
// persistence and presentation conversion belong to an outer adapter.
class SidebarDisplayPreferencesPort
{
public:
    virtual ~SidebarDisplayPreferencesPort() = default;

    [[nodiscard]] virtual SidebarDisplayPreferencesResult load() const = 0;

    [[nodiscard]] virtual SidebarDisplayPreferencesSaveResult save(
        const SidebarDisplayPreferencesSaveRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application

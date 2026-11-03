#pragma once

#include "next/application/dialog_geometry_preferences_port.h"
#include "next/platform/settings_manager_dialog_geometry_preferences_port.h"
#include "ui/shared/dialogs/dialog_shell.h"

#include <string>
#include <string_view>

namespace ClassMngr::Next::Ui
{

// Bind the stateless platform adapter through the Next application port to
// the shared UI's callback-only geometry boundary.
[[nodiscard]] inline DialogGeometryPreferencesCallbacks
settingsManagerDialogGeometryPreferencesCallbacks()
{
    return {
        [](std::string_view stableDialogKey)
        {
            const Platform::SettingsManagerDialogGeometryPreferencesPort adapter;
            const Application::DialogGeometryPreferencesPort& port = adapter;
            return port.read(stableDialogKey);
        },
        [](
            std::string_view stableDialogKey,
            std::string_view geometryPayload
            )
        {
            const Platform::SettingsManagerDialogGeometryPreferencesPort adapter;
            const Application::DialogGeometryPreferencesPort& port = adapter;
            port.write(stableDialogKey, geometryPayload);
        }
    };
}

} // namespace ClassMngr::Next::Ui

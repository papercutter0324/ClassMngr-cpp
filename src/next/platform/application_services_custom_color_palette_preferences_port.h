#pragma once

#include "data/database/workspace_store_fwd.h"
#include "core/application_services.h"
#include "next/application/custom_color_palette_preferences.h"
#include "next/platform/custom_color_palette_preference_codec.h"

#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Compatibility adapter retained for feature code that still owns
// ApplicationServices. The Next startup path uses the direct store adapter.
class ApplicationServicesCustomColorPalettePreferencesPort final
    : public Application::CustomColorPalettePreferencesPort
{
public:
    explicit ApplicationServicesCustomColorPalettePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesCustomColorPalettePreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_store(services ? services->workspaceStore() : nullptr)
    {
    }

    ApplicationServicesCustomColorPalettePreferencesPort(
        const ApplicationServicesCustomColorPalettePreferencesPort& ) = delete;
    ApplicationServicesCustomColorPalettePreferencesPort& operator=(
        const ApplicationServicesCustomColorPalettePreferencesPort& ) = delete;
    ApplicationServicesCustomColorPalettePreferencesPort(
        ApplicationServicesCustomColorPalettePreferencesPort&& ) = delete;
    ApplicationServicesCustomColorPalettePreferencesPort& operator=(
        ApplicationServicesCustomColorPalettePreferencesPort&& ) = delete;

    [[nodiscard]] Domain::Result<Application::CustomColorPalette> read()
        const override;

    [[nodiscard]] Domain::Result<void> write(
        const Application::CustomColorPalette& palette
        ) const override;

    [[nodiscard]] static Application::CustomColorPalette normalizeStoredValue(
        const QVariant& value
        )
    {
        return CustomColorPalettePreferenceCodec::normalizeStoredValue(value);
    }

private:
    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform

#pragma once

#include "data/database/workspace_store_fwd.h"
#include "next/application/custom_color_palette_preferences.h"

namespace ClassMngr::Next::Platform
{

// Direct settings adapter for the pending setup WorkspaceStore. It has no
// ApplicationServices or legacy runtime dependency.
class WorkspaceStoreCustomColorPalettePreferencesPort final
    : public Application::CustomColorPalettePreferencesPort
{
public:
    explicit WorkspaceStoreCustomColorPalettePreferencesPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    WorkspaceStoreCustomColorPalettePreferencesPort(
        const WorkspaceStoreCustomColorPalettePreferencesPort& ) = delete;
    WorkspaceStoreCustomColorPalettePreferencesPort& operator=(
        const WorkspaceStoreCustomColorPalettePreferencesPort& ) = delete;
    WorkspaceStoreCustomColorPalettePreferencesPort(
        WorkspaceStoreCustomColorPalettePreferencesPort&& ) = delete;
    WorkspaceStoreCustomColorPalettePreferencesPort& operator=(
        WorkspaceStoreCustomColorPalettePreferencesPort&& ) = delete;

    [[nodiscard]] Domain::Result<Application::CustomColorPalette> read()
        const override;

    [[nodiscard]] Domain::Result<void> write(
        const Application::CustomColorPalette& palette
        ) const override;

private:
    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform

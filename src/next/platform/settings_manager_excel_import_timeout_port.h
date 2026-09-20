#pragma once

#include "core/settingsmanager.h"
#include "next/application/excel_import_timeout_preferences.h"

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the bounded Excel import timeout preference. The
// legacy SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed read/write contract.
class SettingsManagerExcelImportTimeoutPort final
    : public Application::ExcelImportTimeoutPreferencesPort
{
public:
    SettingsManagerExcelImportTimeoutPort() = default;

    [[nodiscard]] Application::ExcelImportTimeoutPreferences read()
        const override
    {
        return {
            .seconds = SettingsManager::instance().excelImportTimeoutSeconds()
        };
    }

    void write(
        const Application::ExcelImportTimeoutPreferences& preferences
        ) const override
    {
        SettingsManager::instance().setExcelImportTimeoutSeconds(
            preferences.normalizedSeconds()
            );
    }
};

} // namespace ClassMngr::Next::Platform

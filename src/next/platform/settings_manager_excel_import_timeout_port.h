#pragma once

#include "core/settingsmanager.h"
#include "next/application/excel_import_timeout_preferences.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the bounded Excel import timeout preference. The
// legacy SettingsManager singleton is intentionally confined to this adapter;
// application callers consume only the typed read/write contract.
class SettingsManagerExcelImportTimeoutPort final
    : public Application::ExcelImportTimeoutPreferencesPort
{
public:
    explicit SettingsManagerExcelImportTimeoutPort(
        SettingsManager::ExcelImportTimeoutPreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

    [[nodiscard]] Application::ExcelImportTimeoutPreferences read()
        const override
    {
        return {
            .choice = Application::excelImportTimeoutSecondsToChoice(
                SettingsManager::instance().excelImportTimeoutSeconds()
                )
        };
    }

    [[nodiscard]] Domain::Result<void> write(
        const Application::ExcelImportTimeoutPreferences& preferences
        ) const override
    {
        if (!Application::isValidExcelImportTimeoutChoice(
                preferences.choice
                ))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Excel import timeout must be 30, 60, 120, or 300 seconds.",
                .recoverable = false
            });
        }

        const QSettings::Status status =
            SettingsManager::instance().writeExcelImportTimeoutPreference(
                Application::excelImportTimeoutChoiceToSeconds(
                    preferences.choice
                    ),
                m_synchronize
                );
        if (status != QSettings::NoError)
        {
            const char* statusName = status == QSettings::AccessError
                ? "AccessError (settings could not be written)"
                : "FormatError (settings contain invalid data)";
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message =
                    std::string(
                        "Unable to save Excel import timeout preference: QSettings "
                        )
                    + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

private:
    SettingsManager::ExcelImportTimeoutPreferenceSync m_synchronize;
};

} // namespace ClassMngr::Next::Platform

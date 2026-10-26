#pragma once

#include "next/application/preferences_snapshot_adapter.h"
#include "next/platform/settings_manager_theme_preferences_port.h"
#include "next/platform/settings_manager_language_preferences_port.h"
#include "next/platform/settings_manager_font_size_preferences_port.h"
#include "next/platform/settings_manager_sidebar_display_preferences_port.h"
#include "next/platform/settings_manager_powerpoint_data_access_notice_port.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"
#include "next/platform/settings_manager_excel_import_timeout_port.h"
#include "next/platform/settings_manager_last_selected_campus_port.h"

namespace ClassMngr::Next::Platform
{

class SettingsManagerShowAllTeachersPreferencesPort final
    : public Application::ShowAllTeachersPreferencesPort
{
public:
    [[nodiscard]] bool read() const override
    {
        return SettingsManager::instance().showAllKoreanTeachers();
    }
    void write(bool enabled) const override
    {
        SettingsManager::instance().setShowAllKoreanTeachers(enabled);
    }
};

// Owns the legacy typed adapters before the aggregate that borrows them.
class SettingsManagerPreferencesPort final
    : public Application::PreferencesStateReadPort,
      public Application::PreferencesSnapshotPersistPort
{
public:
    SettingsManagerPreferencesPort()
        : m_adapter({m_theme, m_language, m_font, m_sidebar, m_teachers,
                     m_powerpoint, m_updates, m_timeout, m_campus})
    {
    }
    SettingsManagerPreferencesPort(const SettingsManagerPreferencesPort&) = delete;
    SettingsManagerPreferencesPort& operator=(const SettingsManagerPreferencesPort&) = delete;

    [[nodiscard]] Application::PreferencesStateReadResult read() const override
    {
        return m_adapter.read();
    }
    [[nodiscard]] Application::PreferencesPersistResult persist(
        const Application::UserPreferencesSnapshot& snapshot) const override
    {
        return m_adapter.persist(snapshot);
    }

private:
    SettingsManagerThemePreferencesPort m_theme;
    SettingsManagerLanguagePreferencesPort m_language;
    SettingsManagerFontSizePreferencesPort m_font;
    SettingsManagerSidebarDisplayPreferencesPort m_sidebar;
    SettingsManagerShowAllTeachersPreferencesPort m_teachers;
    SettingsManagerPowerPointDataAccessNoticePort m_powerpoint;
    SettingsManagerAutomaticUpdatePreferencesPort m_updates;
    SettingsManagerExcelImportTimeoutPort m_timeout;
    SettingsManagerLastSelectedCampusPort m_campus;
    Application::PreferencesSnapshotAdapter m_adapter;
};

}

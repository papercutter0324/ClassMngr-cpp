#pragma once

#include "next/application/preferences_model.h"
#include "next/application/theme_preferences_port.h"
#include "next/application/language_preferences_port.h"
#include "next/application/sidebar_display_preferences.h"
#include "next/application/automatic_update_preferences.h"
#include "next/application/powerpoint_data_access_notice_preferences.h"
#include "next/application/last_selected_campus_port.h"
#include "next/application/show_all_teachers_preferences.h"

namespace ClassMngr::Next::Application
{

// All ports are borrowed and must outlive this adapter.
struct PreferencesPorts final
{
    ThemePreferencesPort& theme;
    LanguagePreferencesPort& language;
    FontSizePreferencesPort& font;
    SidebarDisplayPreferencesPort& sidebar;
    ShowAllTeachersPreferencesPort& teachers;
    PowerPointDataAccessNoticePreferencesPort& powerpoint;
    AutomaticUpdatePreferencesPort& updates;
    ExcelImportTimeoutPreferencesPort& timeout;
    LastSelectedCampusPort& campus;
};

class PreferencesSnapshotAdapter final
    : public PreferencesStateReadPort, public PreferencesSnapshotPersistPort
{
public:
    explicit PreferencesSnapshotAdapter(PreferencesPorts ports)
        : m_ports(ports)
    {
    }

    [[nodiscard]] PreferencesStateReadResult read() const override
    {
        UserPreferencesState state;
        ThemePreference theme = ThemePreference::SystemDefault;
        switch (m_ports.theme.read())
        {
        case Theme::SystemDefault: break;
        case Theme::Light: theme = ThemePreference::Light; break;
        case Theme::Dark: theme = ThemePreference::Dark; break;
        default:
            return PreferencesStateReadResult::failure(
                UserPreferencesStateDetail::invalidInput("Invalid typed theme preference."));
        }
        const auto sidebar = m_ports.sidebar.load();
        if (!sidebar)
        {
            return PreferencesStateReadResult::failure(sidebar.error());
        }
        // Concrete readers retain their existing default/conversion policy.
        // Validate again at the state boundary rather than importing raw data.
        const auto timeout = m_ports.timeout.read();
        if (!isValidExcelImportTimeoutChoice(timeout.choice))
        {
            return PreferencesStateReadResult::failure(
                UserPreferencesStateDetail::invalidInput("Invalid typed Excel import timeout."));
        }
        const Domain::Result<void> results[] = {
            state.setThemePreference(theme),
            state.setLanguagePreference(m_ports.language.read()),
            state.setFontSize(m_ports.font.read()),
            state.setSidebarTooltipsEnabled(sidebar.value().sidebarTooltipsEnabled),
            state.setSidebarMarqueeEnabled(sidebar.value().sidebarMarqueeEnabled),
            state.setShowAllTeachers(m_ports.teachers.read()),
            state.setShowPowerPointDataAccessNotice(
                m_ports.powerpoint.read().showPowerPointDataAccessNotice),
            state.setAutomaticUpdateChecksEnabled(m_ports.updates.read().automaticChecksEnabled),
            state.setExcelImportTimeoutSeconds(
                excelImportTimeoutChoiceToSeconds(timeout.choice)),
            state.setLastSelectedCampusId(m_ports.campus.read())
        };
        for (const auto& result : results)
        {
            if (!result)
            {
                return PreferencesStateReadResult::failure(result.error());
            }
        }
        return PreferencesStateReadResult::success(std::move(state));
    }

    // Sequential, non-atomic writes stop on the first reported failure.
    // Earlier fields remain written. Retry the full snapshot idempotently.
    // Teacher and PowerPoint writers return void: success cannot verify them.
    [[nodiscard]] PreferencesPersistResult persist(
        const UserPreferencesSnapshot& snapshot) const override
    {
        Theme theme = Theme::SystemDefault;
        switch (snapshot.themePreference())
        {
        case ThemePreference::SystemDefault: break;
        case ThemePreference::Light: theme = Theme::Light; break;
        case ThemePreference::Dark: theme = Theme::Dark; break;
        }
        auto result = m_ports.theme.write(theme);
        if (!result)
        {
            return result;
        }
        result = m_ports.language.write(snapshot.languagePreference());
        if (!result)
        {
            return result;
        }
        result = m_ports.font.write(snapshot.fontSize());
        if (!result)
        {
            return result;
        }
        result = m_ports.sidebar.save({
            snapshot.sidebarTooltipsEnabled(), snapshot.sidebarMarqueeEnabled()
        });
        if (!result)
        {
            return result;
        }
        m_ports.teachers.write(snapshot.showAllTeachers());
        m_ports.powerpoint.write({snapshot.showPowerPointDataAccessNotice()});
        result = m_ports.updates.write({snapshot.automaticUpdateChecksEnabled()});
        if (!result)
        {
            return result;
        }
        result = m_ports.timeout.write({snapshot.excelImportTimeoutChoice()});
        if (!result)
        {
            return result;
        }
        return m_ports.campus.write(snapshot.lastSelectedCampusId());
    }

private:
    PreferencesPorts m_ports;
};

}

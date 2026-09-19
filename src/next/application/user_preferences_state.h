#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Application
{

// The legacy settings store currently accepts these timeout choices. The
// explicit bounds make the adapter-facing contract easy to validate without
// exposing the legacy storage representation.
inline constexpr int kMinimumExcelImportTimeoutSeconds = 30;
inline constexpr int kMaximumExcelImportTimeoutSeconds = 300;
inline constexpr int kDefaultExcelImportTimeoutSeconds = 120;

// Descriptive aliases keep the bounds discoverable at import call sites.
inline constexpr int kMinExcelImportTimeoutSeconds =
    kMinimumExcelImportTimeoutSeconds;
inline constexpr int kMaxExcelImportTimeoutSeconds =
    kMaximumExcelImportTimeoutSeconds;

inline constexpr std::size_t kMaximumLastSelectedCampusIdLength = 256;

enum class ThemePreference
{
    SystemDefault = 0,
    Light = 1,
    Dark = 2
};

enum class LanguagePreference
{
    SystemDefault = 0,
    English = 1,
    Korean = 2
};

namespace UserPreferencesStateDetail
{

[[nodiscard]] constexpr bool isValidThemePreference(
    const ThemePreference preference
    ) noexcept
{
    switch (preference)
    {
    case ThemePreference::SystemDefault:
    case ThemePreference::Light:
    case ThemePreference::Dark:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool isValidLanguagePreference(
    const LanguagePreference preference
    ) noexcept
{
    switch (preference)
    {
    case LanguagePreference::SystemDefault:
    case LanguagePreference::English:
    case LanguagePreference::Korean:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool isSupportedExcelImportTimeoutSeconds(
    const int seconds
    ) noexcept
{
    switch (seconds)
    {
    case 30:
    case 60:
    case 120:
    case 300:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] inline bool isBlank(
    const std::string_view value
    ) noexcept
{
    return value.empty()
        || std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
}

[[nodiscard]] inline bool isValidCampusId(
    const Domain::CampusId& campusId
    ) noexcept
{
    return campusId.value().size() <= kMaximumLastSelectedCampusIdLength
        && !isBlank(campusId.value());
}

[[nodiscard]] inline Domain::OperationError invalidInput(
    const char* message
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    };
}

}

// A copyable value of all application-level preferences. It contains only
// typed values and standard-library storage; persistence and presentation
// conversion belong to a later outer adapter.
class UserPreferencesSnapshot final
{
public:
    UserPreferencesSnapshot() = default;

    [[nodiscard]] ThemePreference themePreference() const noexcept
    {
        return m_themePreference;
    }

    [[nodiscard]] LanguagePreference languagePreference() const noexcept
    {
        return m_languagePreference;
    }

    [[nodiscard]] bool sidebarTooltipsEnabled() const noexcept
    {
        return m_sidebarTooltipsEnabled;
    }

    [[nodiscard]] bool sidebarMarqueeEnabled() const noexcept
    {
        return m_sidebarMarqueeEnabled;
    }

    [[nodiscard]] bool showAllTeachers() const noexcept
    {
        return m_showAllTeachers;
    }

    // This alias keeps the legacy meaning explicit while the application
    // contract uses the shorter, feature-neutral name.
    [[nodiscard]] bool showAllKoreanTeachers() const noexcept
    {
        return showAllTeachers();
    }

    [[nodiscard]] bool showPowerPointDataAccessNotice() const noexcept
    {
        return m_showPowerPointDataAccessNotice;
    }

    [[nodiscard]] bool showPowerPointNotice() const noexcept
    {
        return showPowerPointDataAccessNotice();
    }

    [[nodiscard]] bool automaticUpdateChecksEnabled() const noexcept
    {
        return m_automaticUpdateChecksEnabled;
    }

    [[nodiscard]] int excelImportTimeoutSeconds() const noexcept
    {
        return m_excelImportTimeoutSeconds;
    }

    [[nodiscard]] int importTimeoutSeconds() const noexcept
    {
        return excelImportTimeoutSeconds();
    }

    // Return a value copy so callers cannot mutate state through a snapshot
    // accessor or retain a reference into the owner.
    [[nodiscard]] std::optional<Domain::CampusId> lastSelectedCampusId() const
    {
        return m_lastSelectedCampusId;
    }

    [[nodiscard]] std::optional<Domain::CampusId> lastSelectedCampus() const
    {
        return lastSelectedCampusId();
    }

    friend bool operator==(
        const UserPreferencesSnapshot&,
        const UserPreferencesSnapshot&
        ) = default;

private:
    friend class UserPreferencesState;

    ThemePreference m_themePreference = ThemePreference::SystemDefault;
    LanguagePreference m_languagePreference = LanguagePreference::SystemDefault;
    bool m_sidebarTooltipsEnabled = true;
    bool m_sidebarMarqueeEnabled = true;
    bool m_showAllTeachers = true;
    bool m_showPowerPointDataAccessNotice = true;
    bool m_automaticUpdateChecksEnabled = true;
    int m_excelImportTimeoutSeconds = kDefaultExcelImportTimeoutSeconds;
    std::optional<Domain::CampusId> m_lastSelectedCampusId;
};

// Owns explicit application preferences only. Setters return structured
// validation results and leave the current snapshot untouched on failure.
// There is no persistence, dirty tracking, notification, or presentation
// behavior in this state owner.
class UserPreferencesState final
{
public:
    UserPreferencesState() = default;

    UserPreferencesState(const UserPreferencesState&) = default;
    UserPreferencesState(UserPreferencesState&&) = default;
    UserPreferencesState& operator=(const UserPreferencesState&) = default;
    UserPreferencesState& operator=(UserPreferencesState&&) = default;

    [[nodiscard]] UserPreferencesSnapshot snapshot() const
    {
        return m_snapshot;
    }

    [[nodiscard]] Domain::Result<void> setThemePreference(
        const ThemePreference preference
        )
    {
        if (!UserPreferencesStateDetail::isValidThemePreference(preference))
        {
            return Domain::Result<void>::failure(
                UserPreferencesStateDetail::invalidInput(
                    "Theme preference must be SystemDefault, Light, or Dark."
                    )
                );
        }

        m_snapshot.m_themePreference = preference;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setLanguagePreference(
        const LanguagePreference preference
        )
    {
        if (!UserPreferencesStateDetail::isValidLanguagePreference(preference))
        {
            return Domain::Result<void>::failure(
                UserPreferencesStateDetail::invalidInput(
                    "Language preference must be SystemDefault, English, or Korean."
                    )
                );
        }

        m_snapshot.m_languagePreference = preference;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setSidebarTooltipsEnabled(
        const bool enabled
        ) noexcept
    {
        m_snapshot.m_sidebarTooltipsEnabled = enabled;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setSidebarMarqueeEnabled(
        const bool enabled
        ) noexcept
    {
        m_snapshot.m_sidebarMarqueeEnabled = enabled;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setShowAllTeachers(
        const bool enabled
        ) noexcept
    {
        m_snapshot.m_showAllTeachers = enabled;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setShowAllKoreanTeachers(
        const bool enabled
        ) noexcept
    {
        return setShowAllTeachers(enabled);
    }

    [[nodiscard]] Domain::Result<void> setShowPowerPointDataAccessNotice(
        const bool enabled
        ) noexcept
    {
        m_snapshot.m_showPowerPointDataAccessNotice = enabled;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setShowPowerPointNotice(
        const bool enabled
        ) noexcept
    {
        return setShowPowerPointDataAccessNotice(enabled);
    }

    [[nodiscard]] Domain::Result<void> setAutomaticUpdateChecksEnabled(
        const bool enabled
        ) noexcept
    {
        m_snapshot.m_automaticUpdateChecksEnabled = enabled;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setExcelImportTimeoutSeconds(
        const int seconds
        )
    {
        if (!UserPreferencesStateDetail::isSupportedExcelImportTimeoutSeconds(
                seconds
                ))
        {
            return Domain::Result<void>::failure(
                UserPreferencesStateDetail::invalidInput(
                    "Excel import timeout must be 30, 60, 120, or 300 seconds."
                    )
                );
        }

        m_snapshot.m_excelImportTimeoutSeconds = seconds;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setImportTimeoutSeconds(
        const int seconds
        )
    {
        return setExcelImportTimeoutSeconds(seconds);
    }

    [[nodiscard]] Domain::Result<void> setLastSelectedCampusId(
        std::optional<Domain::CampusId> campusId
        )
    {
        if (campusId.has_value()
            && !UserPreferencesStateDetail::isValidCampusId(*campusId))
        {
            return Domain::Result<void>::failure(
                UserPreferencesStateDetail::invalidInput(
                    "Last selected campus identifier must be non-blank and bounded."
                    )
                );
        }

        m_snapshot.m_lastSelectedCampusId = std::move(campusId);
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> setLastSelectedCampusId(
        Domain::CampusId campusId
        )
    {
        return setLastSelectedCampusId(
            std::optional<Domain::CampusId>(std::move(campusId))
            );
    }

    [[nodiscard]] Domain::Result<void> setLastSelectedCampus(
        Domain::CampusId campusId
        )
    {
        return setLastSelectedCampusId(std::move(campusId));
    }

    [[nodiscard]] Domain::Result<void> clearLastSelectedCampusId() noexcept
    {
        m_snapshot.m_lastSelectedCampusId.reset();
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> clearLastSelectedCampus() noexcept
    {
        return clearLastSelectedCampusId();
    }

    friend bool operator==(
        const UserPreferencesState&,
        const UserPreferencesState&
        ) = default;

private:
    UserPreferencesSnapshot m_snapshot;
};

using PreferencesSnapshot = UserPreferencesSnapshot;
using PreferencesState = UserPreferencesState;

}

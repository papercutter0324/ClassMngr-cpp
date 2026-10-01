#pragma once

#include "next/application/automatic_update_preferences.h"

namespace ClassMngr::Next::Application
{

[[nodiscard]] constexpr bool automaticUpdateStartupCheckIsEligible(
    const bool checkOnStartup,
    const AutomaticUpdatePreferences& preferences,
    const bool hasReleasesApiUrl
    ) noexcept
{
    return checkOnStartup
        && preferences.automaticChecksEnabled
        && hasReleasesApiUrl;
}

} // namespace ClassMngr::Next::Application

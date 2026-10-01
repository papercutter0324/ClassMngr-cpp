#include "next/application/automatic_update_startup_eligibility.h"

#include <array>
#include <cstdlib>

using ClassMngr::Next::Application::AutomaticUpdatePreferences;
using ClassMngr::Next::Application::automaticUpdateStartupCheckIsEligible;

namespace
{

struct EligibilityCase
{
    bool checkOnStartup;
    bool automaticChecksEnabled;
    bool hasReleasesApiUrl;
    bool expected;
};

constexpr std::array<EligibilityCase, 8> EligibilityCases{{
    {false, false, false, false},
    {false, false, true, false},
    {false, true, false, false},
    {false, true, true, false},
    {true, false, false, false},
    {true, false, true, false},
    {true, true, false, false},
    {true, true, true, true}
}};

} // namespace

int main()
{
    for (const EligibilityCase& testCase : EligibilityCases)
    {
        const bool eligible = automaticUpdateStartupCheckIsEligible(
            testCase.checkOnStartup,
            AutomaticUpdatePreferences{
                .automaticChecksEnabled = testCase.automaticChecksEnabled
            },
            testCase.hasReleasesApiUrl
            );
        if (eligible != testCase.expected)
        {
            return EXIT_FAILURE;
        }
    }

    // Recompute eligibility after a disabled preference or missing URL is
    // corrected so those previous inputs do not block a later attempt.
    if (
        automaticUpdateStartupCheckIsEligible(
            true,
            AutomaticUpdatePreferences{.automaticChecksEnabled = false},
            true
            )
        || !automaticUpdateStartupCheckIsEligible(
            true,
            AutomaticUpdatePreferences{.automaticChecksEnabled = true},
            true
            )
        || automaticUpdateStartupCheckIsEligible(
            true,
            AutomaticUpdatePreferences{.automaticChecksEnabled = true},
            false
            )
        || !automaticUpdateStartupCheckIsEligible(
            true,
            AutomaticUpdatePreferences{.automaticChecksEnabled = true},
            true
            )
        )
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

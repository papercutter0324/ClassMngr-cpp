#include "next/application/skipped_update_version_policy.h"

#include <array>
#include <cstdlib>
#include <string_view>

using ClassMngr::Next::Application::SkippedUpdateVersionPolicyInput;
using ClassMngr::Next::Application::SkippedUpdateVersionState;
using ClassMngr::Next::Application::UpdateVersion;
using ClassMngr::Next::Application::decideSkippedUpdateVersion;

namespace
{

struct PolicyCase final
{
    SkippedUpdateVersionState state;
    UpdateVersion current;
    UpdateVersion latest;
    UpdateVersion skipped;
    std::string_view storedText;
    std::string_view latestText;
    bool expectedClear;
    bool expectedMatch;
    bool expectedSuppress;
};

constexpr std::array<PolicyCase, 8> PolicyCases{{
    {
        SkippedUpdateVersionState::Missing,
        {1, 0, 0}, {2, 0, 0}, {0, 0, 0}, {}, "2.0.0",
        false, false, false
    },
    {
        SkippedUpdateVersionState::Invalid,
        {1, 0, 0}, {2, 0, 0}, {0, 0, 0}, "not-a-version", "2.0.0",
        true, false, false
    },
    {
        SkippedUpdateVersionState::Valid,
        {2, 0, 0}, {3, 0, 0}, {2, 0, 0}, "2.0.0", "3.0.0",
        true, false, false
    },
    {
        SkippedUpdateVersionState::Valid,
        {2, 0, 0}, {3, 0, 0}, {1, 9, 9}, "1.9.9", "3.0.0",
        true, false, false
    },
    {
        SkippedUpdateVersionState::Valid,
        {1, 0, 0}, {3, 0, 0}, {2, 9, 9}, "2.9.9", "3.0.0",
        true, false, false
    },
    {
        SkippedUpdateVersionState::Valid,
        {1, 0, 0}, {2, 0, 0}, {2, 0, 0}, "2.0.0", "2.0.0",
        false, true, true
    },
    {
        SkippedUpdateVersionState::Valid,
        {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, "3.0.0", "2.0.0",
        false, false, false
    },
    {
        SkippedUpdateVersionState::Valid,
        {1, 0, 0}, {1, 2, 3}, {1, 2, 3}, "01.2.3", "1.2.3",
        false, false, false
    }
}};

} // namespace

int main()
{
    for (const PolicyCase& testCase : PolicyCases)
    {
        const auto decision = decideSkippedUpdateVersion(
            SkippedUpdateVersionPolicyInput{
                .state = testCase.state,
                .currentVersion = testCase.current,
                .latestVersion = testCase.latest,
                .skippedVersion = testCase.skipped,
                .storedVersionText = testCase.storedText,
                .latestVersionText = testCase.latestText
            }
            );

        if (
            decision.clearStoredVersion != testCase.expectedClear
            || decision.exactMatchToLatest != testCase.expectedMatch
            || decision.suppressAutomaticPrompt != testCase.expectedSuppress
            )
        {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

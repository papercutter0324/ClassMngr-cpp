#pragma once

#include <compare>
#include <string_view>

namespace ClassMngr::Next::Application
{

struct UpdateVersion final
{
    int major = 0;
    int minor = 0;
    int patch = 0;

    friend constexpr auto operator<=>(
        const UpdateVersion&,
        const UpdateVersion&
        ) noexcept = default;
};

enum class SkippedUpdateVersionState
{
    Missing,
    Invalid,
    Valid
};

struct SkippedUpdateVersionPolicyInput final
{
    SkippedUpdateVersionState state = SkippedUpdateVersionState::Missing;
    UpdateVersion currentVersion;
    UpdateVersion latestVersion;
    UpdateVersion skippedVersion;
    std::string_view storedVersionText;
    std::string_view latestVersionText;
};

struct SkippedUpdateVersionPolicyDecision final
{
    bool clearStoredVersion = false;
    bool exactMatchToLatest = false;
    bool suppressAutomaticPrompt = false;
};

// Numeric parsing stays at the Qt boundary. Text matching stays literal so a
// parseable value such as "01.2.3" does not match the canonical "1.2.3".
[[nodiscard]] constexpr SkippedUpdateVersionPolicyDecision
decideSkippedUpdateVersion(
    const SkippedUpdateVersionPolicyInput& input
    ) noexcept
{
    if (input.state == SkippedUpdateVersionState::Missing)
    {
        return {};
    }

    if (input.state == SkippedUpdateVersionState::Invalid)
    {
        return {
            .clearStoredVersion = true
        };
    }

    const bool exactMatchToLatest =
        !input.latestVersionText.empty()
        && input.storedVersionText == input.latestVersionText;

    if (
        input.skippedVersion <= input.currentVersion
        || input.latestVersion > input.skippedVersion
        )
    {
        return {
            .clearStoredVersion = true,
            .exactMatchToLatest = exactMatchToLatest
        };
    }

    return {
        .clearStoredVersion = false,
        .exactMatchToLatest = exactMatchToLatest,
        .suppressAutomaticPrompt = exactMatchToLatest
    };
}

} // namespace ClassMngr::Next::Application

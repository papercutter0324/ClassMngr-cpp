#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class PreferredNameSelectionContext
{
    Load,
    Edit
};

// Choices are supplied in their final UI order and are already normalized.
// Matching is exact and case-sensitive; this policy only selects an index.
[[nodiscard]] inline std::optional<std::size_t>
selectPreferredNameChoiceIndex(
    const std::vector<std::u16string>& orderedChoices,
    const std::u16string& storedPreferredName,
    const std::u16string& currentSelection,
    const PreferredNameSelectionContext context
    )
{
    if (orderedChoices.empty())
    {
        return std::nullopt;
    }

    const auto findExact = [&orderedChoices](const std::u16string& value)
        -> std::optional<std::size_t>
    {
        const auto match = std::find(
            orderedChoices.begin(),
            orderedChoices.end(),
            value
            );
        if (match == orderedChoices.end())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(
            std::distance(orderedChoices.begin(), match)
            );
    };

    if (context == PreferredNameSelectionContext::Edit)
    {
        if (const auto currentMatch = findExact(currentSelection))
        {
            return currentMatch;
        }
    }

    if (const auto storedMatch = findExact(storedPreferredName))
    {
        return storedMatch;
    }

    return std::size_t{0};
}

} // namespace ClassMngr::Next::Application

#pragma once

#include "next/application/evaluation_default_policy_preferences.h"

#include <cstdint>
#include <optional>

namespace ClassMngr::Next::Application
{

// The cycle consumed by default evaluation selection. Calendar dates and
// calendar term types stay at the calendar feature boundary.
enum class EvaluationPeriod : std::uint8_t
{
    Winter,
    Spring,
    Summer,
    Fall
};

// Selects the current evaluation when it has content, otherwise its previous
// cycle entry. The All policy intentionally produces no selection.
[[nodiscard]] constexpr std::optional<EvaluationPeriod>
selectDefaultEvaluationPeriod(
    EvaluationDefaultPolicy policy,
    EvaluationPeriod currentPeriod,
    bool currentEvaluationIsPopulated
    ) noexcept
{
    if (policy != EvaluationDefaultPolicy::CurrentOrPreviousTerm)
    {
        return std::nullopt;
    }

    switch (currentPeriod)
    {
    case EvaluationPeriod::Winter:
        return currentEvaluationIsPopulated
            ? EvaluationPeriod::Winter
            : EvaluationPeriod::Fall;
    case EvaluationPeriod::Spring:
        return currentEvaluationIsPopulated
            ? EvaluationPeriod::Spring
            : EvaluationPeriod::Winter;
    case EvaluationPeriod::Summer:
        return currentEvaluationIsPopulated
            ? EvaluationPeriod::Summer
            : EvaluationPeriod::Spring;
    case EvaluationPeriod::Fall:
        return currentEvaluationIsPopulated
            ? EvaluationPeriod::Fall
            : EvaluationPeriod::Summer;
    }

    return std::nullopt;
}

} // namespace ClassMngr::Next::Application

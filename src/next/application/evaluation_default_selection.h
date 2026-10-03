#pragma once

#include "next/application/evaluation_default_policy_preferences.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

inline constexpr std::array<std::u16string_view, 4> kStoredEvaluationNames{
    u"Winter",
    u"Speech Contest",
    u"Summer",
    u"Fall"
};

// Stored evaluation names are exact, untrimmed values. A period that cannot
// be mapped to a stored evaluation has no name.
[[nodiscard]] constexpr std::u16string_view storedEvaluationName(
    EvaluationPeriod period
    ) noexcept
{
    switch (period)
    {
    case EvaluationPeriod::Winter: return kStoredEvaluationNames[0];
    case EvaluationPeriod::Spring: return kStoredEvaluationNames[1];
    case EvaluationPeriod::Summer: return kStoredEvaluationNames[2];
    case EvaluationPeriod::Fall: return kStoredEvaluationNames[3];
    }

    return {};
}

// Preserves exact-match semantics. Unknown and empty stored values select the
// first canonical evaluation.
[[nodiscard]] constexpr std::u16string_view normalizeStoredEvaluationName(
    std::u16string_view evaluationName
    ) noexcept
{
    for (const std::u16string_view storedName : kStoredEvaluationNames)
    {
        if (evaluationName == storedName)
        {
            return storedName;
        }
    }

    return kStoredEvaluationNames[0];
}

using EvaluationRows = std::vector<std::vector<std::u16string>>;

// QString::trimmed().isEmpty() treats ASCII whitespace, U+0085 NEXT LINE, and
// Unicode separator characters as whitespace. Keep the same locale-independent
// cell-content rule in this Qt-independent contract, using the UTF-16 rows
// returned by the read query.
[[nodiscard]] constexpr bool isEvaluationWhitespace(
    const char16_t value
    ) noexcept
{
    return (value >= u'\t' && value <= u'\r')
        || value == u' '
        || value == u'\u0085'
        || value == u'\u00A0'
        || value == u'\u1680'
        || (value >= u'\u2000' && value <= u'\u200A')
        || value == u'\u2028'
        || value == u'\u2029'
        || value == u'\u202F'
        || value == u'\u205F'
        || value == u'\u3000';
}

[[nodiscard]] constexpr bool evaluationRowsHaveContent(
    const EvaluationRows& rows
    ) noexcept
{
    for (const auto& row : rows)
    {
        for (const std::u16string& cell : row)
        {
            for (const char16_t value : cell)
            {
                if (!isEvaluationWhitespace(value))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

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

[[nodiscard]] constexpr std::optional<EvaluationPeriod>
selectDefaultEvaluationPeriod(
    EvaluationDefaultPolicy policy,
    EvaluationPeriod currentPeriod,
    const EvaluationRows& currentEvaluationRows
    ) noexcept
{
    return selectDefaultEvaluationPeriod(
        policy,
        currentPeriod,
        evaluationRowsHaveContent(currentEvaluationRows)
        );
}

} // namespace ClassMngr::Next::Application

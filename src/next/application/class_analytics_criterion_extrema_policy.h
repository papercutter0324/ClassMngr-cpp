#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassAnalyticsCriterionExtremaSelection final
{
    std::vector<std::size_t> strongestIndices;
    std::vector<std::size_t> focusIndices;
};

// Criteria must already contain the hasData and rounded average facts.
// Thousandths values compare exactly; legacy floating projections retain the
// existing strict tie tolerance.
template <typename Criteria>
[[nodiscard]] inline ClassAnalyticsCriterionExtremaSelection
selectClassAnalyticsCriterionExtrema(const Criteria& criteria)
{
    using Average = std::remove_cvref_t<
        decltype((*criteria.begin()).average3)>;
    constexpr bool hasThousandths = requires(const Average& average) {
        average.thousandths();
    };
    using Key = std::conditional_t<hasThousandths, int, double>;

    const auto keyFor = [](const Average& average) -> Key
    {
        if constexpr (hasThousandths)
        {
            return average.thousandths();
        }
        else
        {
            return average;
        }
    };
    const auto tied = [](const Key left, const Key right)
    {
        if constexpr (hasThousandths)
        {
            return left == right;
        }
        else
        {
            return std::abs(left - right) < 1e-9;
        }
    };

    std::optional<Key> strongestAverage;
    std::optional<Key> focusAverage;
    for (const auto& criterion : criteria)
    {
        if (!criterion.hasData)
        {
            continue;
        }

        const Key average = keyFor(criterion.average3);
        if (!strongestAverage || average > *strongestAverage)
        {
            strongestAverage = average;
        }
        if (!focusAverage || average < *focusAverage)
        {
            focusAverage = average;
        }
    }

    ClassAnalyticsCriterionExtremaSelection selection;
    if (!strongestAverage)
    {
        return selection;
    }

    std::size_t index = 0;
    for (const auto& criterion : criteria)
    {
        if (criterion.hasData)
        {
            const Key average = keyFor(criterion.average3);
            if (tied(average, *strongestAverage))
            {
                selection.strongestIndices.push_back(index);
            }
            if (tied(average, *focusAverage))
            {
                selection.focusIndices.push_back(index);
            }
        }
        ++index;
    }

    return selection;
}

} // namespace ClassMngr::Next::Application

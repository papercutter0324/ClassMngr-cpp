#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassAnalyticsCriterionExtremaSelection final
{
    std::vector<std::size_t> strongestIndices;
    std::vector<std::size_t> focusIndices;
};

// Criteria must already contain the hasData and rounded average facts. Keep
// the strict tolerance aligned with the existing Class Analytics selection.
template <typename Criteria>
[[nodiscard]] inline ClassAnalyticsCriterionExtremaSelection
selectClassAnalyticsCriterionExtrema(const Criteria& criteria)
{
    constexpr double tieTolerance = 1e-9;

    std::optional<double> strongestAverage;
    std::optional<double> focusAverage;
    for (const auto& criterion : criteria)
    {
        if (!criterion.hasData)
        {
            continue;
        }

        if (!strongestAverage || criterion.average3 > *strongestAverage)
        {
            strongestAverage = criterion.average3;
        }
        if (!focusAverage || criterion.average3 < *focusAverage)
        {
            focusAverage = criterion.average3;
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
            if (std::abs(criterion.average3 - *strongestAverage)
                < tieTolerance)
            {
                selection.strongestIndices.push_back(index);
            }
            if (std::abs(criterion.average3 - *focusAverage) < tieTolerance)
            {
                selection.focusIndices.push_back(index);
            }
        }
        ++index;
    }

    return selection;
}

} // namespace ClassMngr::Next::Application

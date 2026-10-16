#pragma once

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

namespace ClassMngr::Next::Application::ClassAnalyticsRankingOrderPolicy
{

// Score keys are prepared by callers from rounded Class Analytics values via
// ClassAnalyticsNumericPolicy::roundToThousandths. Their valid range is 0..5000.
// The input position is the source index retained in the returned ordering.
template <typename NameCompare>
[[nodiscard]] inline std::vector<std::size_t> orderByScoreThenName(
    const std::vector<int>& preparedScoreKeys,
    NameCompare compareNames
    )
{
    std::vector<std::size_t> sourceIndexes(preparedScoreKeys.size());
    std::iota(sourceIndexes.begin(), sourceIndexes.end(), std::size_t{0});

    std::sort(
        sourceIndexes.begin(), sourceIndexes.end(),
        [&preparedScoreKeys, &compareNames](
            const std::size_t left,
            const std::size_t right
            )
        {
            const int leftScore = preparedScoreKeys[left];
            const int rightScore = preparedScoreKeys[right];
            if (leftScore != rightScore)
            {
                return leftScore > rightScore;
            }
            return compareNames(left, right) < 0;
        });

    return sourceIndexes;
}

} // namespace ClassMngr::Next::Application::ClassAnalyticsRankingOrderPolicy

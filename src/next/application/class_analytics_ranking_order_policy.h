#pragma once

#include <algorithm>
#include <iterator>

namespace ClassMngr::Next::Application::ClassAnalyticsRankingOrderPolicy
{

// The score projection returns integer-thousandth keys prepared from rounded
// Class Analytics values via ClassAnalyticsNumericPolicy::roundToThousandths;
// their valid range is 0..5000.
template <std::random_access_iterator RankIterator,
          typename ScoreKeyProjection,
          typename NameCompare>
inline void sortByScoreThenName(
    RankIterator first,
    RankIterator last,
    ScoreKeyProjection scoreKey,
    NameCompare compareNames
    )
{
    std::sort(
        first, last,
        [&scoreKey, &compareNames](const auto& left, const auto& right)
        {
            const int leftScore = scoreKey(left);
            const int rightScore = scoreKey(right);
            if (leftScore != rightScore)
            {
                return leftScore > rightScore;
            }
            return compareNames(left, right) < 0;
        });
}

} // namespace ClassMngr::Next::Application::ClassAnalyticsRankingOrderPolicy

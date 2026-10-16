#include "next/application/class_analytics_ranking_order_policy.h"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

using ClassMngr::Next::Application::ClassAnalyticsRankingOrderPolicy::
    sortByScoreThenName;

namespace
{

struct RankingEntry final
{
    int preparedScoreKey = 0;
    std::string englishName;
};

} // namespace

int main()
{
    // The 5000 score beats 4999 despite its later name. The tie at 3000 uses
    // the supplied comparator to put Bea before Zoe.
    std::vector<RankingEntry> rankings{
        { 3000, "Zoe" }, { 4999, "Amy" }, { 3000, "Bea" }, { 5000, "Zed" }
    };
    bool comparedDifferentScores = false;
    int nameComparisonCount = 0;
    sortByScoreThenName(
        rankings.begin(), rankings.end(),
        [](const RankingEntry& rank) { return rank.preparedScoreKey; },
        [&](const RankingEntry& left, const RankingEntry& right)
        {
            if (left.preparedScoreKey != right.preparedScoreKey)
            {
                comparedDifferentScores = true;
            }
            ++nameComparisonCount;
            if (left.englishName < right.englishName) return -1;
            if (left.englishName > right.englishName) return 1;
            return 0;
        });

    const std::vector<RankingEntry> expected{
        { 5000, "Zed" }, { 4999, "Amy" }, { 3000, "Bea" }, { 3000, "Zoe" }
    };
    if (rankings.size() != expected.size()
        || comparedDifferentScores
        || nameComparisonCount == 0)
    {
        return EXIT_FAILURE;
    }
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (rankings[index].preparedScoreKey != expected[index].preparedScoreKey
            || rankings[index].englishName != expected[index].englishName)
        {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

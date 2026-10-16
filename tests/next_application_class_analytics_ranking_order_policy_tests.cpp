#include "next/application/class_analytics_ranking_order_policy.h"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

using ClassMngr::Next::Application::ClassAnalyticsRankingOrderPolicy::
    orderByScoreThenName;

int main()
{
    // Source 3 beats source 1 by one thousandth, despite its later name.
    // The equal-score pair at 3000 uses the supplied name comparator.
    const std::vector<int> scoreKeys{ 3000, 4999, 3000, 5000 };
    const std::vector<std::string> names{ "Zoe", "Amy", "Bea", "Zed" };
    bool comparedDifferentScores = false;
    int nameComparisonCount = 0;
    const std::vector<std::size_t> order = orderByScoreThenName(
        scoreKeys,
        [&](const std::size_t left, const std::size_t right)
        {
            if (scoreKeys[left] != scoreKeys[right])
            {
                comparedDifferentScores = true;
            }
            ++nameComparisonCount;
            if (names[left] < names[right]) return -1;
            if (names[left] > names[right]) return 1;
            return 0;
        });

    const std::vector<std::size_t> expected{ 3, 1, 2, 0 };
    if (order != expected || comparedDifferentScores || nameComparisonCount == 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

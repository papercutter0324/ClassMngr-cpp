#pragma once

#include "next/application/class_summary_projection.h"
#include "next/application/navigation_order_rank.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Application
{

// Prepared row facts keep feature-owned presentation work outside this
// policy. The label comparator is injected by the UI owner so this header
// stays independent of Qt and locale choices.
struct SubPrepVisibleClassOrderingFact final
{
    std::int32_t configuredLevelOrder =
        kClassSummaryUnknownNavigationOrder;
    ClassTabNavigationOrderRank navigationFirstDayOrder;
    ClassTabNavigationOrderRank navigationFirstTimeOrder;
    std::string baseLabel;
    Domain::ClassId classId;
};

using SubPrepVisibleClassLabelComparator =
    std::function<int(std::string_view, std::string_view)>;

// The caller supplies a non-empty label comparator that returns a negative,
// zero, or positive result. This returns input row indexes ordered by
// configured level, navigation day, navigation minute, injected base-label
// comparison, and class ID. Numeric sentinels compare literally; stable
// sorting preserves input order when the complete tuple is equal.
[[nodiscard]] inline std::vector<std::size_t>
orderSubPrepVisibleClassRows(
    const std::vector<SubPrepVisibleClassOrderingFact>& rows,
    const SubPrepVisibleClassLabelComparator& compareBaseLabels
    )
{
    std::vector<std::size_t> rowOrder(rows.size());
    std::iota(rowOrder.begin(), rowOrder.end(), std::size_t{0});

    std::stable_sort(
        rowOrder.begin(),
        rowOrder.end(),
        [&rows, &compareBaseLabels](
            const std::size_t leftIndex,
            const std::size_t rightIndex
        )
        {
            const auto& left = rows[leftIndex];
            const auto& right = rows[rightIndex];
            if (left.configuredLevelOrder != right.configuredLevelOrder)
            {
                return left.configuredLevelOrder
                    < right.configuredLevelOrder;
            }
            if (
                left.navigationFirstDayOrder.rawValue()
                != right.navigationFirstDayOrder.rawValue()
                )
            {
                return left.navigationFirstDayOrder.rawValue()
                    < right.navigationFirstDayOrder.rawValue();
            }
            if (
                left.navigationFirstTimeOrder.rawValue()
                != right.navigationFirstTimeOrder.rawValue()
                )
            {
                return left.navigationFirstTimeOrder.rawValue()
                    < right.navigationFirstTimeOrder.rawValue();
            }

            const int labelComparison = compareBaseLabels(
                left.baseLabel,
                right.baseLabel
                );
            if (labelComparison != 0)
            {
                return labelComparison < 0;
            }
            return left.classId < right.classId;
        }
        );

    return rowOrder;
}

} // namespace ClassMngr::Next::Application

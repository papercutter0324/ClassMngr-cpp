#pragma once

#include "next/application/class_summary_projection.h"
#include "next/application/navigation_order_rank.h"
#include "next/application/sub_prep_configured_level_order.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
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
    SubPrepConfiguredLevelOrder configuredLevelOrder;
    ClassTabNavigationOrderRank navigationFirstDayOrder;
    ClassTabNavigationOrderRank navigationFirstTimeOrder;
    std::string baseLabel;
    Domain::ClassId classId;
};

using SubPrepVisibleClassLabelComparator =
    std::function<int(std::string_view, std::string_view)>;

class SubPrepVisibleClassRowIndex final
{
public:
    constexpr SubPrepVisibleClassRowIndex() noexcept = default;

    explicit constexpr SubPrepVisibleClassRowIndex(
        const std::size_t rawValue
        ) noexcept
        : rawValue_(rawValue)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return rawValue_;
    }

    friend constexpr bool operator==(
        const SubPrepVisibleClassRowIndex&,
        const SubPrepVisibleClassRowIndex&
        ) noexcept = default;

private:
    std::size_t rawValue_ = 0;
};

// The caller supplies a non-empty label comparator that returns a negative,
// zero, or positive result. This returns input row indexes ordered by
// configured level, navigation day, navigation minute, injected base-label
// comparison, and class ID. Numeric sentinels compare literally; stable
// sorting preserves input order when the complete tuple is equal.
[[nodiscard]] inline std::vector<SubPrepVisibleClassRowIndex>
orderSubPrepVisibleClassRows(
    const std::vector<SubPrepVisibleClassOrderingFact>& rows,
    const SubPrepVisibleClassLabelComparator& compareBaseLabels
    )
{
    std::vector<SubPrepVisibleClassRowIndex> rowOrder;
    rowOrder.reserve(rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        rowOrder.emplace_back(index);
    }

    std::stable_sort(
        rowOrder.begin(),
        rowOrder.end(),
        [&rows, &compareBaseLabels](
            const SubPrepVisibleClassRowIndex leftIndex,
            const SubPrepVisibleClassRowIndex rightIndex
        )
        {
            const auto& left = rows[leftIndex.rawValue()];
            const auto& right = rows[rightIndex.rawValue()];
            if (
                left.configuredLevelOrder.rawValue()
                != right.configuredLevelOrder.rawValue()
                )
            {
                return left.configuredLevelOrder.rawValue()
                    < right.configuredLevelOrder.rawValue();
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

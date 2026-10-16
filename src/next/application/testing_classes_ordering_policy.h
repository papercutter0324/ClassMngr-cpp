#pragma once

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

// Rank resolution and locale-aware text comparison stay with the feature
// edge. This policy orders the prepared facts and returns their input indexes.
struct TestingClassesOrderingFact final
{
    std::int32_t gradeRank;
    std::int32_t levelRank;
    std::u16string levelText;
    std::u16string className;
};

using TestingClassesOrderingTextComparator =
    std::function<int(std::u16string_view, std::u16string_view)>;

// The caller supplies a non-empty comparator that returns a negative, zero,
// or positive result. Rows compare by grade rank, level rank, level text,
// then class name. Exact ties have no ordering guarantee.
[[nodiscard]] inline std::vector<std::size_t>
orderTestingClassesRows(
    const std::vector<TestingClassesOrderingFact>& rows,
    const TestingClassesOrderingTextComparator& compareText
    )
{
    std::vector<std::size_t> rowOrder(rows.size());
    std::iota(rowOrder.begin(), rowOrder.end(), std::size_t{0});

    std::sort(
        rowOrder.begin(),
        rowOrder.end(),
        [&rows, &compareText](
            const std::size_t leftIndex,
            const std::size_t rightIndex
        )
        {
            const auto& left = rows[leftIndex];
            const auto& right = rows[rightIndex];
            if (left.gradeRank != right.gradeRank)
            {
                return left.gradeRank < right.gradeRank;
            }
            if (left.levelRank != right.levelRank)
            {
                return left.levelRank < right.levelRank;
            }

            const int levelComparison = compareText(
                left.levelText,
                right.levelText
                );
            if (levelComparison != 0)
            {
                return levelComparison < 0;
            }
            return compareText(left.className, right.className) < 0;
        }
        );

    return rowOrder;
}

} // namespace ClassMngr::Next::Application

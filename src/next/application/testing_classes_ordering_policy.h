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

class TestingClassesOrderedRowIndex final
{
public:
    constexpr TestingClassesOrderedRowIndex() noexcept = default;
    explicit constexpr TestingClassesOrderedRowIndex(
        const std::size_t value
        ) noexcept : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const TestingClassesOrderedRowIndex&,
        const TestingClassesOrderedRowIndex&
        ) noexcept = default;

private:
    std::size_t m_value = 0;
};

// The caller supplies a non-empty comparator that returns a negative, zero,
// or positive result. Rows compare by grade rank, level rank, level text,
// then class name. Exact ties have no ordering guarantee.
[[nodiscard]] inline std::vector<TestingClassesOrderedRowIndex>
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

    std::vector<TestingClassesOrderedRowIndex> orderedRows;
    orderedRows.reserve(rowOrder.size());
    for (const std::size_t index : rowOrder)
    {
        orderedRows.emplace_back(index);
    }
    return orderedRows;
}

} // namespace ClassMngr::Next::Application

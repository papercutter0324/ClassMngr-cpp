#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

// Rank and label preparation stay at the feature edge. This policy compares
// only the prepared tuple, with the feature supplying its locale-aware label
// comparator.
struct ClassTabNavigationOrderingFact final
{
    std::int32_t gradeRank = 1'000;
    std::int32_t levelRank = 1'000;
    std::int32_t earliestScheduleDayRank = 1'000;
    std::int32_t earliestStartMinuteRank = 1'000;
    std::u16string baseLabel;
    std::int32_t classId = -1;
};

// The caller supplies a label comparator that returns a negative, zero, or
// positive result. Exact tuple ties compare false in both directions.
template <typename CompareBaseLabels>
[[nodiscard]] bool classTabNavigationFactLessThan(
    const ClassTabNavigationOrderingFact& left,
    const ClassTabNavigationOrderingFact& right,
    const CompareBaseLabels& compareBaseLabels
    )
{
    if (left.gradeRank != right.gradeRank)
    {
        return left.gradeRank < right.gradeRank;
    }
    if (left.levelRank != right.levelRank)
    {
        return left.levelRank < right.levelRank;
    }
    if (
        left.earliestScheduleDayRank
        != right.earliestScheduleDayRank
        )
    {
        return left.earliestScheduleDayRank
            < right.earliestScheduleDayRank;
    }
    if (
        left.earliestStartMinuteRank
        != right.earliestStartMinuteRank
        )
    {
        return left.earliestStartMinuteRank
            < right.earliestStartMinuteRank;
    }

    const int labelComparison = compareBaseLabels(
        std::u16string_view(left.baseLabel),
        std::u16string_view(right.baseLabel)
        );
    if (labelComparison != 0)
    {
        return labelComparison < 0;
    }

    return left.classId < right.classId;
}

} // namespace ClassMngr::Next::Application

#pragma once

#include "next/domain/domain_types.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

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
    Domain::ClassId classId = Domain::ClassId::fromString("-1").value();
};

[[nodiscard]] inline bool classTabNavigationClassIdLessThan(
    const Domain::ClassId& left,
    const Domain::ClassId& right
    )
{
    const std::string_view leftValue(left.value());
    const std::string_view rightValue(right.value());
    std::int32_t leftNumber = 0;
    std::int32_t rightNumber = 0;
    const auto leftResult = std::from_chars(
        leftValue.data(),
        leftValue.data() + leftValue.size(),
        leftNumber
        );
    const auto rightResult = std::from_chars(
        rightValue.data(),
        rightValue.data() + rightValue.size(),
        rightNumber
        );
    const bool leftParsed = leftResult.ec == std::errc{}
        && leftResult.ptr == leftValue.data() + leftValue.size();
    const bool rightParsed = rightResult.ec == std::errc{}
        && rightResult.ptr == rightValue.data() + rightValue.size();

    if (leftParsed != rightParsed)
    {
        return leftParsed;
    }
    if (leftParsed && leftNumber != rightNumber)
    {
        return leftNumber < rightNumber;
    }

    return leftValue < rightValue;
}

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

    return classTabNavigationClassIdLessThan(left.classId, right.classId);
}

} // namespace ClassMngr::Next::Application

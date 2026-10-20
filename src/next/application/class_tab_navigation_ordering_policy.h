#pragma once

#include "next/domain/domain_types.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace ClassMngr::Next::Application
{

class ClassTabNavigationOrderRank final
{
public:
    constexpr ClassTabNavigationOrderRank() noexcept = default;

    explicit constexpr ClassTabNavigationOrderRank(
        const std::int32_t rawValue
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr std::int32_t rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const ClassTabNavigationOrderRank&,
        const ClassTabNavigationOrderRank&
        ) noexcept = default;

private:
    std::int32_t m_rawValue = 1'000;
};

// Rank and label preparation stay at the feature edge. This policy compares
// only the prepared tuple, with the feature supplying its locale-aware label
// comparator.
struct ClassTabNavigationOrderingFact final
{
    ClassTabNavigationOrderRank gradeRank;
    ClassTabNavigationOrderRank levelRank;
    ClassTabNavigationOrderRank earliestScheduleDayRank;
    ClassTabNavigationOrderRank earliestStartMinuteRank;
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
    if (left.gradeRank.rawValue() != right.gradeRank.rawValue())
    {
        return left.gradeRank.rawValue() < right.gradeRank.rawValue();
    }
    if (left.levelRank.rawValue() != right.levelRank.rawValue())
    {
        return left.levelRank.rawValue() < right.levelRank.rawValue();
    }
    if (
        left.earliestScheduleDayRank.rawValue()
        != right.earliestScheduleDayRank.rawValue()
        )
    {
        return left.earliestScheduleDayRank.rawValue()
            < right.earliestScheduleDayRank.rawValue();
    }
    if (
        left.earliestStartMinuteRank.rawValue()
        != right.earliestStartMinuteRank.rawValue()
        )
    {
        return left.earliestStartMinuteRank.rawValue()
            < right.earliestStartMinuteRank.rawValue();
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

#pragma once

#include "next/application/class_analytics_average_thousandths.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ClassMngr::Next::Application::ClassAnalyticsNumericPolicy
{

inline constexpr int kThousandthsPerPoint = 1000;
inline constexpr int kGradeAdvanceThresholdThousandths = 400;

// Analytics averages are finite values in [0, 5]. Rounding is positive
// half-up, expressed as an integer so grade conversion uses the same rounded
// value as the dashboard displays.
[[nodiscard]] inline int roundToThousandths(const double average) noexcept
{
    return static_cast<int>(std::floor(
        average * kThousandthsPerPoint + 0.5));
}

[[nodiscard]] inline double roundTo3(const double average) noexcept
{
    return static_cast<double>(roundToThousandths(average))
        / kThousandthsPerPoint;
}

// Average already-rounded student values without reintroducing floating-point
// representation error at a half-thousandth boundary.
[[nodiscard]] inline int roundMeanOfThousandths(
    const std::int64_t sumThousandths,
    const std::size_t count
    ) noexcept
{
    if (count == 0) return 0;

    const std::int64_t divisor = static_cast<std::int64_t>(count);
    const std::int64_t quotient = sumThousandths / divisor;
    const std::int64_t remainder = sumThousandths % divisor;
    const std::int64_t halfUpRemainder = divisor / 2 + divisor % 2;
    return static_cast<int>(
        quotient + (remainder >= halfUpRemainder ? 1 : 0));
}

[[nodiscard]] inline int roundAverageToGrade(
    const ClassAnalyticsAverageThousandths average
    ) noexcept
{
    const int rounded = average.thousandths();
    if (rounded <= 0) return 0;
    if (rounded >= 5 * kThousandthsPerPoint) return 5;

    const int whole = rounded / kThousandthsPerPoint;
    const int fraction = rounded % kThousandthsPerPoint;
    return whole + (fraction >= kGradeAdvanceThresholdThousandths ? 1 : 0);
}

[[nodiscard]] inline int roundAverageToGrade(const double average) noexcept
{
    if (average <= 0.0) return 0;
    if (average >= 5.0) return 5;

    const int rounded = roundToThousandths(average);
    return roundAverageToGrade(ClassAnalyticsAverageThousandths{rounded});
}

} // namespace ClassMngr::Next::Application::ClassAnalyticsNumericPolicy

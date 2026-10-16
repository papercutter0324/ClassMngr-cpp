#pragma once

#include <cmath>

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

[[nodiscard]] inline int roundAverageToGrade(const double average) noexcept
{
    if (average <= 0.0) return 0;
    if (average >= 5.0) return 5;

    const int rounded = roundToThousandths(average);
    const int whole = rounded / kThousandthsPerPoint;
    const int fraction = rounded % kThousandthsPerPoint;
    return whole + (fraction >= kGradeAdvanceThresholdThousandths ? 1 : 0);
}

} // namespace ClassMngr::Next::Application::ClassAnalyticsNumericPolicy

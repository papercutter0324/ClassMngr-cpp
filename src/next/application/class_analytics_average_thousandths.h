#pragma once

namespace ClassMngr::Next::Application
{

class ClassAnalyticsAverageThousandths final
{
public:
    constexpr ClassAnalyticsAverageThousandths() noexcept = default;
    explicit constexpr ClassAnalyticsAverageThousandths(
        const int thousandths
        ) noexcept
        : m_thousandths(thousandths)
    {
    }

    [[nodiscard]] constexpr int thousandths() const noexcept
    {
        return m_thousandths;
    }

    [[nodiscard]] constexpr double toDouble() const noexcept
    {
        return static_cast<double>(m_thousandths) / 1000.0;
    }

    friend constexpr bool operator==(
        const ClassAnalyticsAverageThousandths&,
        const ClassAnalyticsAverageThousandths&
        ) noexcept = default;

private:
    int m_thousandths = 0;
};

} // namespace ClassMngr::Next::Application

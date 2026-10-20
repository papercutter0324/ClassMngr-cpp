#pragma once

namespace ClassMngr::Next::Application
{

class ClassAnalyticsCriterionStudentCount final
{
public:
    constexpr ClassAnalyticsCriterionStudentCount() noexcept = default;
    explicit constexpr ClassAnalyticsCriterionStudentCount(
        const int value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const ClassAnalyticsCriterionStudentCount&,
        const ClassAnalyticsCriterionStudentCount&
        ) noexcept = default;

private:
    int m_value = 0;
};

} // namespace ClassMngr::Next::Application

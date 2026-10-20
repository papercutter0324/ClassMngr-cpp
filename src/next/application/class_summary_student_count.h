#pragma once

#include <cstddef>

namespace ClassMngr::Next::Application
{

class ClassSummaryStudentCount final
{
public:
    constexpr ClassSummaryStudentCount() noexcept = default;

    explicit constexpr ClassSummaryStudentCount(
        const std::size_t rawValue
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const ClassSummaryStudentCount&,
        const ClassSummaryStudentCount&
        ) noexcept = default;

private:
    std::size_t m_rawValue = 0;
};

}

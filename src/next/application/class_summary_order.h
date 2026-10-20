#pragma once

#include <cstdint>

namespace ClassMngr::Next::Application
{

class ClassSummaryOrder final
{
public:
    constexpr ClassSummaryOrder() noexcept = default;

    explicit constexpr ClassSummaryOrder(
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
        const ClassSummaryOrder&,
        const ClassSummaryOrder&
        ) noexcept = default;

private:
    std::int32_t m_rawValue = 0;
};

} // namespace ClassMngr::Next::Application

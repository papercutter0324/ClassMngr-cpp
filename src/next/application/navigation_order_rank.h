#pragma once

#include <cstdint>

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

} // namespace ClassMngr::Next::Application

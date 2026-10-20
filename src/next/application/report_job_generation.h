#pragma once

#include <cstdint>

namespace ClassMngr::Next::Application
{

class ReportJobGeneration final
{
public:
    constexpr ReportJobGeneration() noexcept = default;

    explicit constexpr ReportJobGeneration(const std::uint64_t value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const ReportJobGeneration&,
        const ReportJobGeneration&
        ) noexcept = default;

private:
    std::uint64_t m_value = 0;
};

} // namespace ClassMngr::Next::Application

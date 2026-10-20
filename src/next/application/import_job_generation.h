#pragma once

#include <cstdint>

namespace ClassMngr::Next::Application
{

class ImportJobGeneration final
{
public:
    constexpr ImportJobGeneration() noexcept = default;

    explicit constexpr ImportJobGeneration(const std::uint64_t value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const ImportJobGeneration&,
        const ImportJobGeneration&
        ) noexcept = default;

private:
    std::uint64_t m_value = 0;
};

} // namespace ClassMngr::Next::Application

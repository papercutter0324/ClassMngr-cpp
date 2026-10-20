#pragma once

#include <cstdint>

namespace ClassMngr::Next::Application
{

// Preserve the exact signed millisecond value at the application boundary.
// This type does not validate, clamp, or assign sentinel meaning to zero.
class UpdateCheckUtcMilliseconds final
{
public:
    constexpr UpdateCheckUtcMilliseconds() noexcept = default;

    explicit constexpr UpdateCheckUtcMilliseconds(std::int64_t value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::int64_t value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const UpdateCheckUtcMilliseconds&,
        const UpdateCheckUtcMilliseconds&
        ) = default;

private:
    std::int64_t m_value = 0;
};

} // namespace ClassMngr::Next::Application

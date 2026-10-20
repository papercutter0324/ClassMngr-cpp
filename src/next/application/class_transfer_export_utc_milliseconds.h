#pragma once

#include <cstdint>

namespace ClassMngr::Next::Application
{

class ClassTransferExportUtcMilliseconds final
{
public:
    constexpr ClassTransferExportUtcMilliseconds() noexcept = default;

    explicit constexpr ClassTransferExportUtcMilliseconds(
        const std::int64_t value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::int64_t value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const ClassTransferExportUtcMilliseconds&,
        const ClassTransferExportUtcMilliseconds&
        ) noexcept = default;

private:
    std::int64_t m_value = 0;
};

} // namespace ClassMngr::Next::Application

#pragma once

#include <cstddef>

namespace ClassMngr::Next::Application
{

class ScheduleImportMatchingClassCount final
{
public:
    constexpr ScheduleImportMatchingClassCount() noexcept = default;

    explicit constexpr ScheduleImportMatchingClassCount(
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
        const ScheduleImportMatchingClassCount&,
        const ScheduleImportMatchingClassCount&
        ) noexcept = default;

private:
    std::size_t m_rawValue = 0;
};

}

#pragma once

#include <cstddef>

namespace ClassMngr::Next::Application
{

class CalendarEventImportCandidateIndex final
{
public:
    constexpr CalendarEventImportCandidateIndex() noexcept = default;

    explicit constexpr CalendarEventImportCandidateIndex(
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
        const CalendarEventImportCandidateIndex&,
        const CalendarEventImportCandidateIndex&
        ) noexcept = default;

private:
    std::size_t m_rawValue = 0;
};

} // namespace ClassMngr::Next::Application

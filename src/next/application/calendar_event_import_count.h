#pragma once

namespace ClassMngr::Next::Application
{

class CalendarEventImportCount final
{
public:
    constexpr CalendarEventImportCount() noexcept = default;

    explicit constexpr CalendarEventImportCount(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    constexpr CalendarEventImportCount& operator++() noexcept
    {
        ++m_value;
        return *this;
    }

    friend constexpr bool operator==(
        const CalendarEventImportCount&,
        const CalendarEventImportCount&
        ) noexcept = default;

private:
    int m_value = 0;
};

} // namespace ClassMngr::Next::Application

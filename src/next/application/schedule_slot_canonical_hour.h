#pragma once

namespace ClassMngr::Next::Application
{

class ScheduleSlotCanonicalHour final
{
public:
    explicit constexpr ScheduleSlotCanonicalHour(
        const int value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const ScheduleSlotCanonicalHour&,
        const ScheduleSlotCanonicalHour&
        ) = default;

private:
    int m_value;
};

} // namespace ClassMngr::Next::Application

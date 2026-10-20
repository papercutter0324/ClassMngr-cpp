#pragma once

namespace ClassMngr::Next::Application
{

class RosterAvailabilityFirstEmptyRow final
{
public:
    constexpr explicit RosterAvailabilityFirstEmptyRow(
        const int rawValue = 0
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_rawValue;
    }

    [[nodiscard]] constexpr bool isFull() const noexcept
    {
        return m_rawValue == -1;
    }

    friend constexpr bool operator==(
        const RosterAvailabilityFirstEmptyRow&,
        const RosterAvailabilityFirstEmptyRow&
        ) noexcept = default;

private:
    int m_rawValue = 0;
};

} // namespace ClassMngr::Next::Application

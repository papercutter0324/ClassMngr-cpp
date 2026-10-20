#pragma once

namespace ClassMngr::Next::Application
{

class AcademicTermWeekCount final
{
public:
    constexpr AcademicTermWeekCount() noexcept = default;

    explicit constexpr AcademicTermWeekCount(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const AcademicTermWeekCount&,
        const AcademicTermWeekCount&
        ) noexcept = default;

private:
    int m_value = 0;
};

} // namespace ClassMngr::Next::Application

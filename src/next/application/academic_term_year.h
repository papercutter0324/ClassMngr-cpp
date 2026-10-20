#pragma once

namespace ClassMngr::Next::Application
{

class AcademicTermYear final
{
public:
    constexpr AcademicTermYear() noexcept = default;

    explicit constexpr AcademicTermYear(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const AcademicTermYear&,
        const AcademicTermYear&
        ) noexcept = default;

private:
    int m_value = 0;
};

} // namespace ClassMngr::Next::Application

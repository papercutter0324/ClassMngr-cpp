#pragma once

namespace ClassMngr::Next::Application
{

class ClassDetailsStudentCount final
{
public:
    constexpr ClassDetailsStudentCount() noexcept = default;

    explicit constexpr ClassDetailsStudentCount(const int rawValue) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const ClassDetailsStudentCount&,
        const ClassDetailsStudentCount&
        ) noexcept = default;

private:
    int m_rawValue = 0;
};

} // namespace ClassMngr::Next::Application

#pragma once

namespace ClassMngr::Next::Application
{

class MyClassesStudentCount final
{
public:
    explicit constexpr MyClassesStudentCount(const int rawValue) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const MyClassesStudentCount&,
        const MyClassesStudentCount&
        ) noexcept = default;

private:
    int m_rawValue;
};

} // namespace ClassMngr::Next::Application

#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Export keys retain exact UTF-16 code units and stay distinct from both
// export text payloads and the UTF-8 keys used by apply.
class ClassExportClassKey final
{
public:
    ClassExportClassKey() = default;

    explicit ClassExportClassKey(std::u16string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    [[nodiscard]] const std::u16string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ClassExportClassKey&,
        const ClassExportClassKey&
        ) = default;

private:
    std::u16string m_value;
};

class ClassExportTeacherKey final
{
public:
    ClassExportTeacherKey() = default;

    explicit ClassExportTeacherKey(std::u16string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    [[nodiscard]] const std::u16string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ClassExportTeacherKey&,
        const ClassExportTeacherKey&
        ) = default;

private:
    std::u16string m_value;
};

} // namespace ClassMngr::Next::Application

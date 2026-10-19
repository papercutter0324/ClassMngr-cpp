#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// These keys preserve the exact adapter-provided bytes. Their constructors do
// not validate or normalize; each owning workflow applies its own rules.
class ClassTransferTeacherKey final
{
public:
    ClassTransferTeacherKey() = default;

    explicit ClassTransferTeacherKey(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ClassTransferTeacherKey&,
        const ClassTransferTeacherKey&
        ) = default;

    friend bool operator<(
        const ClassTransferTeacherKey& left,
        const ClassTransferTeacherKey& right
        )
    {
        return left.m_value < right.m_value;
    }

private:
    std::string m_value;
};

class ClassTransferClassKey final
{
public:
    ClassTransferClassKey() = default;

    explicit ClassTransferClassKey(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ClassTransferClassKey&,
        const ClassTransferClassKey&
        ) = default;

    friend bool operator<(
        const ClassTransferClassKey& left,
        const ClassTransferClassKey& right
        )
    {
        return left.m_value < right.m_value;
    }

private:
    std::string m_value;
};

} // namespace ClassMngr::Next::Application

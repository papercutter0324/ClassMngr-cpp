#pragma once

namespace ClassMngr::Next::Application
{
class ClassTransferPackageVersion final
{
public:
    explicit ClassTransferPackageVersion(int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] int value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ClassTransferPackageVersion&,
        const ClassTransferPackageVersion&
        ) = default;

private:
    int m_value;
};
} // namespace ClassMngr::Next::Application

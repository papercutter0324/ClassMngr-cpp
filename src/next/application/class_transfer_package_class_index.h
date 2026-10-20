#pragma once

namespace ClassMngr::Next::Application
{

class ClassTransferPackageClassIndex final
{
public:
    constexpr ClassTransferPackageClassIndex() noexcept = default;

    explicit constexpr ClassTransferPackageClassIndex(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const ClassTransferPackageClassIndex&,
        const ClassTransferPackageClassIndex&
        ) noexcept = default;

private:
    int m_value = -1;
};

} // namespace ClassMngr::Next::Application

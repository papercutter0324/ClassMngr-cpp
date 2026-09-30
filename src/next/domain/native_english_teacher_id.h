#pragma once

namespace ClassMngr::Next::Domain
{

class NativeEnglishTeacherId final
{
public:
    explicit constexpr NativeEnglishTeacherId(
        const int value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const NativeEnglishTeacherId&,
        const NativeEnglishTeacherId&
        ) = default;

private:
    int m_value;
};

} // namespace ClassMngr::Next::Domain

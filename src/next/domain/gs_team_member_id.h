#pragma once

namespace ClassMngr::Next::Domain
{

class GsTeamMemberId final
{
public:
    explicit constexpr GsTeamMemberId(
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
        const GsTeamMemberId&,
        const GsTeamMemberId&
        ) = default;

private:
    int m_value;
};

} // namespace ClassMngr::Next::Domain

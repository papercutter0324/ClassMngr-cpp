#pragma once

namespace ClassMngr::Next::Application
{

// Campus editing is a feature-local capability. It is intentionally not a
// navigation grant and does not affect any other page or service.
class CampusEditCapability final
{
public:
    [[nodiscard]] static constexpr CampusEditCapability readOnly() noexcept
    {
        return CampusEditCapability(false);
    }

    [[nodiscard]] static constexpr CampusEditCapability enabled() noexcept
    {
        return CampusEditCapability(true);
    }

    [[nodiscard]] constexpr bool canEdit() const noexcept
    {
        return m_enabled;
    }

    friend bool operator==(
        const CampusEditCapability&,
        const CampusEditCapability&
        ) = default;

private:
    explicit constexpr CampusEditCapability(bool enabled) noexcept
        : m_enabled(enabled)
    {
    }

    bool m_enabled = false;
};

} // namespace ClassMngr::Next::Application

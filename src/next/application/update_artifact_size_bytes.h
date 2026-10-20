#pragma once

#include <cstdint>

namespace ClassMngr::Next::Application
{

// Preserve the signed artifact size exactly. This value does not validate or
// clamp the size.
class UpdateArtifactSizeBytes final
{
public:
    constexpr UpdateArtifactSizeBytes() noexcept = default;

    explicit constexpr UpdateArtifactSizeBytes(std::int64_t value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::int64_t value() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const UpdateArtifactSizeBytes&,
        const UpdateArtifactSizeBytes&
        ) = default;

private:
    std::int64_t m_value = 0;
};

} // namespace ClassMngr::Next::Application

#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Preserve the platform adapter's UTF-8 bytes unchanged. This value does not
// validate, normalize, or map platform keys.
class UpdateArtifactPlatformKey final
{
public:
    UpdateArtifactPlatformKey() = default;

    explicit UpdateArtifactPlatformKey(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const UpdateArtifactPlatformKey&,
        const UpdateArtifactPlatformKey&
        ) = default;

private:
    std::string m_value;
};

} // namespace ClassMngr::Next::Application

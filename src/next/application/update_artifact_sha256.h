#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Preserve artifact digest text exactly. This value does not validate or
// normalize the string.
class UpdateArtifactSha256 final
{
public:
    UpdateArtifactSha256() = default;

    explicit UpdateArtifactSha256(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const UpdateArtifactSha256&,
        const UpdateArtifactSha256&
        ) = default;

private:
    std::string m_value;
};

} // namespace ClassMngr::Next::Application

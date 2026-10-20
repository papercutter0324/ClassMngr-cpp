#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Preserve artifact filename text exactly. This value does not validate,
// normalize, or interpret the filename.
class UpdateArtifactFileName final
{
public:
    UpdateArtifactFileName() = default;

    explicit UpdateArtifactFileName(std::string rawValue)
        : m_rawValue(std::move(rawValue))
    {
    }

    [[nodiscard]] const std::string& rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend bool operator==(
        const UpdateArtifactFileName&,
        const UpdateArtifactFileName&
        ) = default;

private:
    std::string m_rawValue;
};

} // namespace ClassMngr::Next::Application

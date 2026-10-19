#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Domain
{

// Owns opaque signature image bytes for a save request. Image preparation
// belongs to the platform boundary.
class PersonalSignatureImageSaveBytes final
{
public:
    PersonalSignatureImageSaveBytes() = default;

    explicit PersonalSignatureImageSaveBytes(std::string bytes)
        : m_bytes(std::move(bytes))
    {
    }

    [[nodiscard]] const std::string& value() const& noexcept
    {
        return m_bytes;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_bytes.empty();
    }

    friend bool operator==(
        const PersonalSignatureImageSaveBytes&,
        const PersonalSignatureImageSaveBytes&
        ) = default;

private:
    std::string m_bytes;
};

} // namespace ClassMngr::Next::Domain

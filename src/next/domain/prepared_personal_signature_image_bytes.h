#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Domain
{

// Owns prepared personal-signature image bytes without interpreting them.
// Empty bytes are valid; image preparation belongs to the platform boundary.
class PreparedPersonalSignatureImageBytes final
{
public:
    PreparedPersonalSignatureImageBytes() = default;

    explicit PreparedPersonalSignatureImageBytes(std::string bytes)
        : m_bytes(std::move(bytes))
    {
    }

    [[nodiscard]] const std::string& bytes() const noexcept
    {
        return m_bytes;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_bytes.empty();
    }

    friend bool operator==(
        const PreparedPersonalSignatureImageBytes&,
        const PreparedPersonalSignatureImageBytes&
        ) = default;

private:
    std::string m_bytes;
};

} // namespace ClassMngr::Next::Domain

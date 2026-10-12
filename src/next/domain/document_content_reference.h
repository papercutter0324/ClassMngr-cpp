#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Domain
{

// Carries a compact token, path, or URI without owning document bytes or a
// viewer/platform document object.
class DocumentContentReference final
{
public:
    using Text = std::string;

    DocumentContentReference() = default;

    explicit DocumentContentReference(
        Text text
        )
        : m_text(std::move(text))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_text.empty();
    }

    [[nodiscard]] const Text& value() const noexcept
    {
        return m_text;
    }

    [[nodiscard]] const Text& text() const noexcept
    {
        return m_text;
    }

    friend bool operator==(
        const DocumentContentReference&,
        const DocumentContentReference&
        ) = default;

private:
    Text m_text;
};

}

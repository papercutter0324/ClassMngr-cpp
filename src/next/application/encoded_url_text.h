#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Preserve encoded URL text exactly. This value does not validate or
// canonicalize its contents.
class EncodedUrlText final
{
public:
    EncodedUrlText() = default;

    explicit EncodedUrlText(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    friend bool operator==(
        const EncodedUrlText&,
        const EncodedUrlText&
        ) = default;

private:
    std::string m_value;
};

} // namespace ClassMngr::Next::Application

#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Preserve ISO date text exactly. This value does not validate or
// canonicalize its contents.
class ReleaseDateIsoText final
{
public:
    ReleaseDateIsoText() = default;

    explicit ReleaseDateIsoText(std::string value)
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
        const ReleaseDateIsoText&,
        const ReleaseDateIsoText&
        ) = default;

private:
    std::string m_value;
};

} // namespace ClassMngr::Next::Application

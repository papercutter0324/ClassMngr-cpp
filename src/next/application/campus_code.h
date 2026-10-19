#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// An owning campus-code value. The exact bytes are retained; interpretation,
// validation, and normalization belong to the boundary that uses the code.
class CampusCode final
{
public:
    explicit CampusCode(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(const CampusCode&, const CampusCode&) = default;

private:
    std::string m_value;
};

} // namespace ClassMngr::Next::Application

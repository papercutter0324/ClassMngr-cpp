#pragma once

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// Carries an unmodified UTF-8 directory preference across the application
// boundary. Empty values represent an unset preference.
class DirectoryPreferencePath final
{
public:
    DirectoryPreferencePath() = default;

    explicit DirectoryPreferencePath(
        std::string value
        )
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const DirectoryPreferencePath&,
        const DirectoryPreferencePath&
        ) = default;

private:
    std::string m_value;
};

} // namespace ClassMngr::Next::Application

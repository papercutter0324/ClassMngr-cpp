#pragma once

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the last database directory as UTF-8.
// Empty or unavailable persistence is represented by an empty string.
class LastDatabaseDirectoryPort
{
public:
    virtual ~LastDatabaseDirectoryPort() = default;

    [[nodiscard]] virtual std::string read() const = 0;

    virtual void write(
        const std::string& directory
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

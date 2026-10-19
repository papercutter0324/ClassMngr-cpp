#pragma once

#include "next/application/directory_preference_path.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries the last database directory as UTF-8.
// Empty or unavailable persistence is represented by an empty string.
class LastDatabaseDirectoryPort
{
public:
    virtual ~LastDatabaseDirectoryPort() = default;

    [[nodiscard]] virtual DirectoryPreferencePath read() const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        const DirectoryPreferencePath& directory
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

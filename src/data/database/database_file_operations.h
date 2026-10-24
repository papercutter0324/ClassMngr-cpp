#pragma once

#include "data/repositories/repository_error.h"

#include <QString>

namespace DatabaseFileOperations
{

[[nodiscard]] RepositoryStatus copyDatabaseFile(
    const QString& sourcePath,
    const QString& destinationPath
    );

} // namespace DatabaseFileOperations

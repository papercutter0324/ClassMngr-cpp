#pragma once

#include "core/result.h"

#include <QString>

namespace DatabaseFileOperations
{

[[nodiscard]] Status copyDatabaseFile(
    const QString& sourcePath,
    const QString& destinationPath
    );

} // namespace DatabaseFileOperations

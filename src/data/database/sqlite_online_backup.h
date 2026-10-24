#pragma once

#include "core/result.h"

#include <QString>

namespace DatabaseFileOperations::Internal
{

// Creates a consistent SQLite image at an unused staging path using the
// independently linked SQLite Online Backup API. The source remains open and
// unchanged; callers own staging-file cleanup and final destination commit.
[[nodiscard]] Status createOnlineBackupSnapshot(
    const QString& sourcePath,
    const QString& stagingPath
    );

} // namespace DatabaseFileOperations::Internal

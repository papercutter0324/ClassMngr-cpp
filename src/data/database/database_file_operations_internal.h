#pragma once

#include "core/result.h"

#include <QFileDevice>
#include <QString>

class QFile;
class QSaveFile;

namespace DatabaseFileOperations::Internal
{

// Copies an already-open source into an already-open QSaveFile, cancelling
// the temporary destination whenever streaming or finalization fails.
[[nodiscard]] Status copyOpenSourceToSaveFile(
    QFile& source,
    QSaveFile& destination,
    const QString& destinationPath,
    bool destinationExisted,
    QFileDevice::Permissions sourcePermissions
    );

} // namespace DatabaseFileOperations::Internal

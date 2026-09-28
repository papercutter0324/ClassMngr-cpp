#include "database_file_operations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

Status DatabaseFileOperations::copyDatabaseFile(
    const QString& sourcePath,
    const QString& destinationPath
    )
{
    if (destinationPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("No destination path was provided.")
            );
    }

    const QString absoluteSourcePath =
        QFileInfo(sourcePath).absoluteFilePath();
    const QFileInfo targetInfo(destinationPath);
    const QString targetPath = targetInfo.absoluteFilePath();

    if (absoluteSourcePath == targetPath)
    {
        return {};
    }

    if (
        !targetInfo.absolutePath().isEmpty()
        && !QDir().mkpath(targetInfo.absolutePath())
        )
    {
        return std::unexpected(
            QStringLiteral("Unable to create destination directory:\n%1")
                .arg(targetInfo.absolutePath())
            );
    }

    if (
        QFile::exists(targetPath)
        && !QFile::remove(targetPath)
        )
    {
        return std::unexpected(
            QStringLiteral("Unable to replace existing Teacher Profile file:\n%1")
                .arg(targetPath)
            );
    }

    if (!QFile::copy(absoluteSourcePath, targetPath))
    {
        return std::unexpected(
            QStringLiteral("Unable to copy Teacher Profile to:\n%1")
                .arg(targetPath)
            );
    }

    return {};
}

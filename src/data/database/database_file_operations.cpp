#include "database_file_operations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <filesystem>
#include <system_error>

namespace
{
std::filesystem::path fileSystemPath(const QString& path)
{
#if defined(Q_OS_WIN)
    return std::filesystem::path(path.toStdWString());
#else
    const QByteArray utf8Path = path.toUtf8();
    return std::filesystem::u8path(utf8Path.constData());
#endif
}

bool pathsReferToSameFile(
    const QString& sourcePath,
    const QString& targetPath
    )
{
    const QString cleanSourcePath = QDir::cleanPath(sourcePath);
    const QString cleanTargetPath = QDir::cleanPath(targetPath);
    if (cleanSourcePath == cleanTargetPath)
    {
        return true;
    }

    const QFileInfo sourceInfo(cleanSourcePath);
    const QFileInfo targetInfo(cleanTargetPath);
    if (!sourceInfo.exists() || !targetInfo.exists())
    {
        return false;
    }

    std::error_code error;
    return std::filesystem::equivalent(
        fileSystemPath(cleanSourcePath),
        fileSystemPath(cleanTargetPath),
        error
        );
}
}

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

    const QString absoluteSourcePath = QDir::cleanPath(
        QFileInfo(sourcePath).absoluteFilePath());
    const QFileInfo targetInfo(destinationPath);
    const QString targetPath = QDir::cleanPath(targetInfo.absoluteFilePath());

    if (pathsReferToSameFile(absoluteSourcePath, targetPath))
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

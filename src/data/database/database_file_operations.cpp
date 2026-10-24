#include "database_file_operations.h"
#include "database_file_operations_internal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace
{
std::filesystem::path fileSystemPath(const QString& path)
{
#if defined(Q_OS_WIN)
    return std::filesystem::path(path.toStdWString());
#else
    const QByteArray utf8Path = path.toUtf8();
    std::u8string pathBytes;
    pathBytes.reserve(static_cast<std::size_t>(utf8Path.size()));
    for (const char byte : utf8Path)
    {
        if (byte == '\0')
        {
            break;
        }
        pathBytes.push_back(static_cast<char8_t>(
            static_cast<unsigned char>(byte)
            ));
    }
    return std::filesystem::path(std::move(pathBytes));
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

Status DatabaseFileOperations::Internal::copyOpenSourceToSaveFile(
    QFile& source,
    QSaveFile& destination,
    const QString& destinationPath,
    const bool destinationExisted,
    const QFileDevice::Permissions sourcePermissions
    )
{
    const QString copyError =
        QStringLiteral("Unable to copy Teacher Profile to:\n%1")
            .arg(destinationPath);
    const QString replacementError =
        QStringLiteral("Unable to replace existing Teacher Profile file:\n%1")
            .arg(destinationPath);

    QByteArray buffer(64 * 1024, '\0');
    while (true)
    {
        const qint64 bytesRead = source.read(buffer.data(), buffer.size());
        if (bytesRead < 0)
        {
            destination.cancelWriting();
            return std::unexpected(copyError);
        }
        if (bytesRead == 0)
        {
            if (source.error() != QFileDevice::NoError)
            {
                destination.cancelWriting();
                return std::unexpected(copyError);
            }
            break;
        }

        qint64 bytesWritten = 0;
        while (bytesWritten < bytesRead)
        {
            const qint64 writeResult = destination.write(
                buffer.constData() + bytesWritten,
                bytesRead - bytesWritten
                );
            if (writeResult <= 0)
            {
                destination.cancelWriting();
                return std::unexpected(copyError);
            }
            bytesWritten += writeResult;
        }
    }

    if (!destination.flush())
    {
        destination.cancelWriting();
        return std::unexpected(copyError);
    }

    if (!destination.setPermissions(sourcePermissions))
    {
        destination.cancelWriting();
        return std::unexpected(copyError);
    }

    if (!destination.commit())
    {
        if (destinationExisted)
        {
            return std::unexpected(replacementError);
        }
        return std::unexpected(copyError);
    }

    return {};
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

    QFile source(absoluteSourcePath);
    const QString copyError =
        QStringLiteral("Unable to copy Teacher Profile to:\n%1")
            .arg(targetPath);
    if (!source.open(QIODevice::ReadOnly))
    {
        return std::unexpected(
            copyError
            );
    }

    const QFileDevice::Permissions sourcePermissions = source.permissions();
    const bool targetExisted = QFile::exists(targetPath);
    const QString replacementError =
        QStringLiteral("Unable to replace existing Teacher Profile file:\n%1")
            .arg(targetPath);
    QSaveFile destination(targetPath);
    destination.setDirectWriteFallback(false);
    if (!destination.open(QIODevice::WriteOnly))
    {
        if (targetExisted)
        {
            return std::unexpected(replacementError);
        }
        return std::unexpected(
            copyError
            );
    }

    return Internal::copyOpenSourceToSaveFile(
        source,
        destination,
        targetPath,
        targetExisted,
        sourcePermissions
        );
}

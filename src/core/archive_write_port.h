#pragma once

#include <QList>
#include <QString>

// Synchronous archive output. Source files remain owned by the caller and
// must stay alive for the call; entries are written in the supplied order.
// On failure, return false and set errorMessage when supplied. The caller
// owns staging cleanup and committing the archive to its final destination.
class ArchiveWritePort
{
public:
    struct Entry
    {
        QString sourcePath;
        QString archiveName;
    };

    virtual ~ArchiveWritePort() = default;
    [[nodiscard]] virtual bool writeArchive(
        const QString& archivePath,
        const QList<Entry>& entries,
        QString* errorMessage = nullptr) = 0;
};

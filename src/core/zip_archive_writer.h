#pragma once

#include <QList>
#include <QString>

namespace ZipArchiveWriter
{

struct Entry
{
    QString sourcePath;
    QString archiveName;
};

[[nodiscard]] bool writeArchive(
    const QString& archivePath,
    const QList<Entry>& entries,
    QString* errorMessage = nullptr
    );

} // namespace ZipArchiveWriter

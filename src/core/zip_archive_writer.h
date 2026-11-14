#pragma once

#include "core/archive_write_port.h"

namespace ZipArchiveWriter
{

using Entry = ArchiveWritePort::Entry;

[[nodiscard]] bool writeArchive(
    const QString& archivePath,
    const QList<Entry>& entries,
    QString* errorMessage = nullptr
    );

} // namespace ZipArchiveWriter

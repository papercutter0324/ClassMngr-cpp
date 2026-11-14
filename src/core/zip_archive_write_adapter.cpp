#include "core/zip_archive_write_adapter.h"
#include "core/zip_archive_writer.h"

namespace
{
class ZipArchiveWriteAdapter final : public ArchiveWritePort
{
public:
    bool writeArchive(const QString& archivePath, const QList<Entry>& entries,
        QString* errorMessage) override
    {
        return ZipArchiveWriter::writeArchive(archivePath, entries, errorMessage);
    }
};
}

std::shared_ptr<ArchiveWritePort> makeZipArchiveWritePort()
{
    return std::make_shared<ZipArchiveWriteAdapter>();
}

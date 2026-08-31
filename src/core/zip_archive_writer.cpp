#include "zip_archive_writer.h"

#include "classmngr/engine/zip_archive_writer.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QObject>
#include <QSet>

#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace ZipArchiveWriter
{
namespace
{

using EngineEntry = classmngr::engine::ZipArchiveWriter::Entry;

std::string toUtf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

void setError(
    QString* errorMessage,
    const QString& message
    )
{
    if (errorMessage)
    {
        *errorMessage = message;
    }
}

QString duplicateArchiveName(
    const QList<Entry>& entries
    )
{
    QSet<QString> archiveNames;
    for (const Entry& entry : entries)
    {
        if (archiveNames.contains(entry.archiveName))
        {
            return entry.archiveName;
        }
        archiveNames.insert(entry.archiveName);
    }
    return {};
}

QString sourceFileName(
    const QList<Entry>& entries,
    std::string_view token
    )
{
    if (token == "source-too-large")
    {
        for (const Entry& entry : entries)
        {
            const QFileInfo sourceInfo(entry.sourcePath);
            if (sourceInfo.size() >= 0
                && static_cast<quint64>(sourceInfo.size())
                    > std::numeric_limits<quint32>::max())
            {
                return sourceInfo.fileName();
            }
        }
    }
    else if (token == "source-open-failed")
    {
        for (const Entry& entry : entries)
        {
            QFile source(entry.sourcePath);
            const QFileInfo sourceInfo(source);
            if (!sourceInfo.exists()
                || !sourceInfo.isFile()
                || !source.open(QIODevice::ReadOnly))
            {
                return sourceInfo.fileName();
            }
        }
    }

    if (!entries.isEmpty())
    {
        return QFileInfo(entries.first().sourcePath).fileName();
    }
    return {};
}

QString localizedError(
    const classmngr::engine::Error& error,
    const QList<Entry>& entries
    )
{
    const std::string_view token = error.message;
    if (token == "empty-entry-list")
    {
        return QObject::tr("There are no files to add to the ZIP archive.");
    }
    if (token == "entry-count-limit")
    {
        return QObject::tr("There are too many files for a standard ZIP archive.");
    }
    if (token == "invalid-entry-name")
    {
        return QObject::tr("A ZIP entry has an invalid file name.");
    }
    if (token == "duplicate-entry-name")
    {
        return QObject::tr(
            "The ZIP archive would contain more than one file named \"%1\"."
            ).arg(duplicateArchiveName(entries));
    }
    if (token == "source-open-failed")
    {
        return QObject::tr("The file \"%1\" could not be opened for archiving.")
            .arg(sourceFileName(entries, token));
    }
    if (token == "source-too-large")
    {
        return QObject::tr(
            "The file \"%1\" is too large for a standard ZIP archive."
            ).arg(sourceFileName(entries, token));
    }
    if (token == "entry-name-too-long")
    {
        return QObject::tr("A ZIP entry file name is too long.");
    }
    if (token == "source-read-failed")
    {
        return QObject::tr("The file \"%1\" could not be read for archiving.")
            .arg(sourceFileName(entries, token));
    }
    if (token == "archive-open-failed")
    {
        return QObject::tr("The ZIP archive could not be created.");
    }
    if (token == "archive-size-limit")
    {
        return QObject::tr(
            "The report files are too large for a standard ZIP archive."
            );
    }
    if (token == "archive-entry-write-failed")
    {
        return QObject::tr("A report could not be written to the ZIP archive.");
    }
    if (token == "archive-directory-write-failed")
    {
        return QObject::tr(
            "The ZIP archive directory could not be written."
            );
    }
    if (token == "archive-finalize-failed")
    {
        return QObject::tr("The ZIP archive could not be finalized.");
    }

    return QObject::tr("The ZIP archive could not be finalized.");
}

} // namespace

bool writeArchive(
    const QString& archivePath,
    const QList<Entry>& entries,
    QString* errorMessage
    )
{
    std::vector<EngineEntry> engineEntries;
    engineEntries.reserve(static_cast<std::size_t>(entries.size()));
    for (const Entry& entry : entries)
    {
        engineEntries.push_back({
            toUtf8(entry.sourcePath),
            toUtf8(entry.archiveName)
        });
    }

    const auto result = classmngr::engine::ZipArchiveWriter::writeArchive(
        toUtf8(archivePath),
        engineEntries
        );
    if (!result)
    {
        setError(
            errorMessage,
            localizedError(result.error(), entries)
            );
        return false;
    }

    return true;
}

} // namespace ZipArchiveWriter

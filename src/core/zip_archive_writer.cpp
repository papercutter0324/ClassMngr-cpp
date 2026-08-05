#include "zip_archive_writer.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QObject>
#include <QSaveFile>
#include <QSet>
#include <QStringList>

#include <zlib.h>

#include <algorithm>
#include <limits>

namespace ZipArchiveWriter
{
namespace
{

constexpr quint32 LocalFileHeaderSignature = 0x04034b50;
constexpr quint32 CentralDirectoryHeaderSignature = 0x02014b50;
constexpr quint32 EndOfCentralDirectorySignature = 0x06054b50;
constexpr quint16 ZipVersion20 = 20;
constexpr quint16 Utf8FileNameFlag = 0x0800;
constexpr qsizetype BufferSize = 64 * 1024;

struct PreparedEntry
{
    QString sourcePath;
    QByteArray archiveName;
    quint32 crc = 0;
    quint32 size = 0;
    quint32 localHeaderOffset = 0;
    quint16 modifiedTime = 0;
    quint16 modifiedDate = 0;
};

void appendLe16(QByteArray* data, quint16 value)
{
    data->append(static_cast<char>(value & 0xff));
    data->append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(QByteArray* data, quint32 value)
{
    appendLe16(data, static_cast<quint16>(value & 0xffff));
    appendLe16(data, static_cast<quint16>((value >> 16) & 0xffff));
}

bool writeBytes(
    QIODevice* device,
    const QByteArray& data
    )
{
    return device
        && device->write(data) == data.size();
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

bool validArchiveName(
    const QString& archiveName
    )
{
    if (archiveName.trimmed().isEmpty()
        || archiveName.startsWith(QLatin1Char('/'))
        || archiveName.contains(QLatin1Char('\\')))
    {
        return false;
    }

    const QStringList components =
        archiveName.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    return std::none_of(
        components.cbegin(),
        components.cend(),
        [](const QString& component)
        {
            return component.isEmpty()
                || component == QStringLiteral(".")
                || component == QStringLiteral("..");
        }
        );
}

void dosDateTime(
    const QDateTime& dateTime,
    quint16* time,
    quint16* date
    )
{
    QDateTime local = dateTime.toLocalTime();
    if (!local.isValid())
    {
        local = QDateTime::currentDateTime();
    }

    const QDate localDate = local.date();
    const QTime localTime = local.time();
    const int year = qBound(1980, localDate.year(), 2107);
    *time = static_cast<quint16>(
        (localTime.hour() << 11)
        | (localTime.minute() << 5)
        | (localTime.second() / 2)
        );
    *date = static_cast<quint16>(
        ((year - 1980) << 9)
        | (localDate.month() << 5)
        | localDate.day()
        );
}

bool prepareEntry(
    const Entry& entry,
    PreparedEntry* prepared,
    QString* errorMessage
    )
{
    if (!prepared || !validArchiveName(entry.archiveName))
    {
        setError(
            errorMessage,
            QObject::tr("A ZIP entry has an invalid file name.")
            );
        return false;
    }

    QFile source(entry.sourcePath);
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.exists()
        || !sourceInfo.isFile()
        || !source.open(QIODevice::ReadOnly))
    {
        setError(
            errorMessage,
            QObject::tr("The file \"%1\" could not be opened for archiving.")
                .arg(sourceInfo.fileName())
            );
        return false;
    }

    if (sourceInfo.size() < 0
        || static_cast<quint64>(sourceInfo.size())
            > std::numeric_limits<quint32>::max())
    {
        setError(
            errorMessage,
            QObject::tr("The file \"%1\" is too large for a standard ZIP archive.")
                .arg(sourceInfo.fileName())
            );
        return false;
    }

    const QByteArray archiveName = entry.archiveName.toUtf8();
    if (archiveName.isEmpty()
        || archiveName.size() > std::numeric_limits<quint16>::max())
    {
        setError(
            errorMessage,
            QObject::tr("A ZIP entry file name is too long.")
            );
        return false;
    }

    uLong crc = crc32(0L, Z_NULL, 0);
    QByteArray buffer(BufferSize, Qt::Uninitialized);
    while (!source.atEnd())
    {
        const qint64 bytesRead =
            source.read(buffer.data(), buffer.size());
        if (bytesRead < 0)
        {
            setError(
                errorMessage,
                QObject::tr("The file \"%1\" could not be read for archiving.")
                    .arg(sourceInfo.fileName())
                );
            return false;
        }
        crc = crc32(
            crc,
            reinterpret_cast<const Bytef*>(buffer.constData()),
            static_cast<uInt>(bytesRead)
            );
    }

    prepared->sourcePath = entry.sourcePath;
    prepared->archiveName = archiveName;
    prepared->crc = static_cast<quint32>(crc);
    prepared->size = static_cast<quint32>(sourceInfo.size());
    dosDateTime(
        sourceInfo.lastModified(),
        &prepared->modifiedTime,
        &prepared->modifiedDate
        );
    return true;
}

QByteArray localHeader(
    const PreparedEntry& entry
    )
{
    QByteArray header;
    header.reserve(30 + entry.archiveName.size());
    appendLe32(&header, LocalFileHeaderSignature);
    appendLe16(&header, ZipVersion20);
    appendLe16(&header, Utf8FileNameFlag);
    appendLe16(&header, 0);
    appendLe16(&header, entry.modifiedTime);
    appendLe16(&header, entry.modifiedDate);
    appendLe32(&header, entry.crc);
    appendLe32(&header, entry.size);
    appendLe32(&header, entry.size);
    appendLe16(
        &header,
        static_cast<quint16>(entry.archiveName.size())
        );
    appendLe16(&header, 0);
    header.append(entry.archiveName);
    return header;
}

QByteArray centralDirectoryHeader(
    const PreparedEntry& entry
    )
{
    QByteArray header;
    header.reserve(46 + entry.archiveName.size());
    appendLe32(&header, CentralDirectoryHeaderSignature);
    appendLe16(&header, ZipVersion20);
    appendLe16(&header, ZipVersion20);
    appendLe16(&header, Utf8FileNameFlag);
    appendLe16(&header, 0);
    appendLe16(&header, entry.modifiedTime);
    appendLe16(&header, entry.modifiedDate);
    appendLe32(&header, entry.crc);
    appendLe32(&header, entry.size);
    appendLe32(&header, entry.size);
    appendLe16(
        &header,
        static_cast<quint16>(entry.archiveName.size())
        );
    appendLe16(&header, 0);
    appendLe16(&header, 0);
    appendLe16(&header, 0);
    appendLe16(&header, 0);
    appendLe32(&header, 0);
    appendLe32(&header, entry.localHeaderOffset);
    header.append(entry.archiveName);
    return header;
}

bool copyFileToDevice(
    const QString& sourcePath,
    QIODevice* destination
    )
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QByteArray buffer(BufferSize, Qt::Uninitialized);
    while (!source.atEnd())
    {
        const qint64 bytesRead =
            source.read(buffer.data(), buffer.size());
        if (bytesRead < 0
            || destination->write(buffer.constData(), bytesRead)
                != bytesRead)
        {
            return false;
        }
    }
    return true;
}

} // namespace

bool writeArchive(
    const QString& archivePath,
    const QList<Entry>& entries,
    QString* errorMessage
    )
{
    if (entries.isEmpty())
    {
        setError(
            errorMessage,
            QObject::tr("There are no files to add to the ZIP archive.")
            );
        return false;
    }
    if (static_cast<quint64>(entries.size())
        > std::numeric_limits<quint16>::max())
    {
        setError(
            errorMessage,
            QObject::tr("There are too many files for a standard ZIP archive.")
            );
        return false;
    }

    QList<PreparedEntry> preparedEntries;
    preparedEntries.reserve(entries.size());
    QSet<QString> archiveNames;
    for (const Entry& entry : entries)
    {
        if (archiveNames.contains(entry.archiveName))
        {
            setError(
                errorMessage,
                QObject::tr("The ZIP archive would contain more than one file named \"%1\".")
                    .arg(entry.archiveName)
                );
            return false;
        }

        PreparedEntry prepared;
        if (!prepareEntry(entry, &prepared, errorMessage))
        {
            return false;
        }
        archiveNames.insert(entry.archiveName);
        preparedEntries.append(prepared);
    }

    QSaveFile archive(archivePath);
    if (!archive.open(QIODevice::WriteOnly))
    {
        setError(
            errorMessage,
            QObject::tr("The ZIP archive could not be created.")
            );
        return false;
    }

    for (PreparedEntry& entry : preparedEntries)
    {
        if (archive.pos() < 0
            || static_cast<quint64>(archive.pos())
                > std::numeric_limits<quint32>::max())
        {
            archive.cancelWriting();
            setError(
                errorMessage,
                QObject::tr("The report files are too large for a standard ZIP archive.")
                );
            return false;
        }
        entry.localHeaderOffset =
            static_cast<quint32>(archive.pos());
        if (!writeBytes(&archive, localHeader(entry))
            || !copyFileToDevice(entry.sourcePath, &archive))
        {
            archive.cancelWriting();
            setError(
                errorMessage,
                QObject::tr("A report could not be written to the ZIP archive.")
                );
            return false;
        }
    }

    if (archive.pos() < 0
        || static_cast<quint64>(archive.pos())
            > std::numeric_limits<quint32>::max())
    {
        archive.cancelWriting();
        setError(
            errorMessage,
            QObject::tr("The report files are too large for a standard ZIP archive.")
            );
        return false;
    }
    const quint32 centralDirectoryOffset =
        static_cast<quint32>(archive.pos());

    for (const PreparedEntry& entry : preparedEntries)
    {
        if (!writeBytes(
                &archive,
                centralDirectoryHeader(entry)
                ))
        {
            archive.cancelWriting();
            setError(
                errorMessage,
                QObject::tr("The ZIP archive directory could not be written.")
                );
            return false;
        }
    }

    const quint64 centralDirectorySize =
        static_cast<quint64>(archive.pos())
        - centralDirectoryOffset;
    if (centralDirectorySize > std::numeric_limits<quint32>::max())
    {
        archive.cancelWriting();
        setError(
            errorMessage,
            QObject::tr("The report files are too large for a standard ZIP archive.")
            );
        return false;
    }

    QByteArray endRecord;
    endRecord.reserve(22);
    appendLe32(&endRecord, EndOfCentralDirectorySignature);
    appendLe16(&endRecord, 0);
    appendLe16(&endRecord, 0);
    appendLe16(
        &endRecord,
        static_cast<quint16>(preparedEntries.size())
        );
    appendLe16(
        &endRecord,
        static_cast<quint16>(preparedEntries.size())
        );
    appendLe32(
        &endRecord,
        static_cast<quint32>(centralDirectorySize)
        );
    appendLe32(&endRecord, centralDirectoryOffset);
    appendLe16(&endRecord, 0);

    if (!writeBytes(&archive, endRecord)
        || !archive.commit())
    {
        setError(
            errorMessage,
            QObject::tr("The ZIP archive could not be finalized.")
            );
        return false;
    }

    return true;
}

} // namespace ZipArchiveWriter

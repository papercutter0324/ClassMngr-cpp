#include "core/zip_archive_read_adapter.h"

#include <QHash>
#include <QIODevice>

#include <zlib.h>

#include <cstring>
#include <limits>
#include <utility>

namespace
{
constexpr int MinZipEocdSize = 22;
constexpr int MaxZipCommentSize = 0xffff;
constexpr quint32 ZipEocdSignature = 0x06054b50;
constexpr quint32 ZipCentralFileSignature = 0x02014b50;
constexpr quint32 ZipLocalFileSignature = 0x04034b50;

struct ZipEntry
{
    QString name;
    quint16 method = 0;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint32 localHeaderOffset = 0;
};

class ZipArchiveSource final
{
public:
    explicit ZipArchiveSource(const QByteArray& bytes)
        : m_bytes(&bytes)
        , m_size(bytes.size())
    {
    }

    explicit ZipArchiveSource(QIODevice& device)
        : m_device(&device)
        , m_size(device.size())
    {
        m_valid = device.isOpen()
            && device.isReadable()
            && !device.isSequential()
            && m_size >= 0;
    }

    [[nodiscard]] qint64 size() const noexcept
    {
        return m_size;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return m_valid;
    }

    [[nodiscard]] bool failed() const noexcept
    {
        return m_failed;
    }

    bool readAt(
        qint64 offset,
        qint64 count,
        QByteArray* result
        ) const
    {
        if (
            !m_valid
            || !result
            || offset < 0
            || count < 0
            || offset > m_size
            || count > m_size - offset
            || count > std::numeric_limits<qsizetype>::max()
            )
        {
            m_failed = true;
            return false;
        }

        if (m_bytes)
        {
            *result = m_bytes->mid(
                static_cast<qsizetype>(offset),
                static_cast<qsizetype>(count)
                );
            return result->size() == count;
        }

        if (!m_device || !m_device->seek(offset))
        {
            m_failed = true;
            return false;
        }

        result->resize(static_cast<qsizetype>(count));
        qint64 totalRead = 0;
        while (totalRead < count)
        {
            const qint64 bytesRead = m_device->read(
                result->data() + totalRead,
                count - totalRead
                );
            if (bytesRead <= 0)
            {
                m_failed = true;
                result->clear();
                return false;
            }
            totalRead += bytesRead;
        }
        return true;
    }

private:
    const QByteArray* m_bytes = nullptr;
    QIODevice* m_device = nullptr;
    qint64 m_size = 0;
    mutable bool m_valid = true;
    mutable bool m_failed = false;
};

quint16 readLe16(
    const QByteArray& data,
    qsizetype offset
    )
{
    if (offset < 0 || offset + 2 > data.size())
    {
        return 0;
    }

    const auto* bytes =
        reinterpret_cast<const uchar*>(data.constData() + offset);
    return static_cast<quint16>(
        bytes[0]
        | (bytes[1] << 8)
        );
}

quint32 readLe32(
    const QByteArray& data,
    qsizetype offset
    )
{
    if (offset < 0 || offset + 4 > data.size())
    {
        return 0;
    }

    const auto* bytes =
        reinterpret_cast<const uchar*>(data.constData() + offset);
    return static_cast<quint32>(
        bytes[0]
        | (bytes[1] << 8)
        | (bytes[2] << 16)
        | (bytes[3] << 24)
        );
}

QHash<QString, ZipEntry> zipEntries(
    const ZipArchiveSource& source
    )
{
    QHash<QString, ZipEntry> entries;
    if (source.size() < MinZipEocdSize)
    {
        return entries;
    }

    const qint64 searchStart = qMax<qint64>(
        0,
        source.size() - MinZipEocdSize - MaxZipCommentSize
        );
    QByteArray tail;
    if (!source.readAt(searchStart, source.size() - searchStart, &tail))
    {
        return entries;
    }

    qsizetype eocdOffset = -1;
    for (qsizetype offset = tail.size() - MinZipEocdSize;
         offset >= 0;
         --offset)
    {
        if (readLe32(tail, offset) == ZipEocdSignature)
        {
            eocdOffset = offset;
            break;
        }
    }

    if (eocdOffset < 0)
    {
        return entries;
    }

    const quint16 entryCount = readLe16(tail, eocdOffset + 10);
    const quint32 centralDirectoryOffset = readLe32(tail, eocdOffset + 16);
    qint64 offset = static_cast<qint64>(centralDirectoryOffset);

    for (int index = 0; index < entryCount; ++index)
    {
        if (offset < 0 || offset > source.size() - 46)
        {
            return {};
        }

        QByteArray centralHeader;
        if (
            !source.readAt(offset, 46, &centralHeader)
            || readLe32(centralHeader, 0) != ZipCentralFileSignature
            )
        {
            return {};
        }

        const quint16 method = readLe16(centralHeader, 10);
        const quint32 compressedSize = readLe32(centralHeader, 20);
        const quint32 uncompressedSize = readLe32(centralHeader, 24);
        const quint16 fileNameLength = readLe16(centralHeader, 28);
        const quint16 extraLength = readLe16(centralHeader, 30);
        const quint16 commentLength = readLe16(centralHeader, 32);
        const quint32 localHeaderOffset = readLe32(centralHeader, 42);
        const qint64 recordLength =
            46LL + fileNameLength + extraLength + commentLength;
        if (recordLength > source.size() - offset)
        {
            return {};
        }

        QByteArray fileName;
        if (!source.readAt(offset + 46, fileNameLength, &fileName))
        {
            return {};
        }

        ZipEntry entry;
        entry.name = QString::fromUtf8(fileName);
        entry.method = method;
        entry.compressedSize = compressedSize;
        entry.uncompressedSize = uncompressedSize;
        entry.localHeaderOffset = localHeaderOffset;
        entries.insert(entry.name, entry);
        offset += recordLength;
    }

    return entries;
}

class ZipArchiveEntryReadDevice final : public ArchiveEntryReadDevice
{
public:
    ZipArchiveEntryReadDevice(
        const ZipArchiveSource& source,
        const ZipEntry& entry
        )
        : m_source(source)
        , m_entry(entry)
    {
        const qint64 localOffset = entry.localHeaderOffset;
        if (localOffset > source.size() - 30)
        {
            setErrorString(QStringLiteral("Invalid ZIP local file header."));
            return;
        }

        QByteArray localHeader;
        if (
            !source.readAt(localOffset, 30, &localHeader)
            || readLe32(localHeader, 0) != ZipLocalFileSignature
            )
        {
            setErrorString(QStringLiteral("Invalid ZIP local file header."));
            return;
        }

        const quint16 fileNameLength = readLe16(localHeader, 26);
        const quint16 extraLength = readLe16(localHeader, 28);
        m_dataOffset = localOffset + 30 + fileNameLength + extraLength;
        if (
            m_dataOffset > source.size()
            || entry.compressedSize > source.size() - m_dataOffset
            || (entry.method != 0 && entry.method != 8)
            )
        {
            setErrorString(QStringLiteral("Invalid ZIP entry data."));
            return;
        }

        m_compressedRemaining = entry.compressedSize;
        if (entry.method == 8)
        {
            if (inflateInit2(&m_stream, -MAX_WBITS) != Z_OK)
            {
                setErrorString(QStringLiteral("Could not initialize ZIP decompression."));
                return;
            }
            m_inflateInitialized = true;
        }

        m_valid = open(QIODevice::ReadOnly);
    }

    ~ZipArchiveEntryReadDevice() override
    {
        if (m_inflateInitialized)
        {
            inflateEnd(&m_stream);
        }
    }

    [[nodiscard]] bool isValid() const noexcept override
    {
        return m_valid;
    }

    [[nodiscard]] bool failed() const noexcept override
    {
        return m_failed;
    }

    [[nodiscard]] bool isSequential() const override
    {
        return true;
    }

protected:
    qint64 readData(char* destination, qint64 maximumSize) override
    {
        if (!m_valid || m_failed || maximumSize <= 0)
        {
            return m_failed ? -1 : 0;
        }

        if (m_entry.method == 0)
        {
            const quint32 remaining =
                m_entry.compressedSize - m_compressedRead;
            if (remaining == 0)
            {
                return 0;
            }

            const qint64 count = qMin<qint64>(
                qMin<qint64>(maximumSize, InputChunkSize),
                remaining
                );
            QByteArray input;
            if (
                !m_source.readAt(
                    m_dataOffset + m_compressedRead,
                    count,
                    &input
                    )
                )
            {
                fail(QStringLiteral("Could not read ZIP entry data."));
                return -1;
            }

            std::memcpy(
                destination,
                input.constData(),
                static_cast<size_t>(count)
                );
            m_compressedRead += static_cast<quint32>(count);
            m_uncompressedRead += static_cast<quint32>(count);
            return count;
        }

        if (m_streamEnded)
        {
            return 0;
        }

        qint64 written = 0;
        while (written == 0 && !m_streamEnded)
        {
            if (m_stream.avail_in == 0 && m_compressedRemaining > 0)
            {
                const quint32 inputSize = qMin<quint32>(
                    m_compressedRemaining,
                    static_cast<quint32>(InputChunkSize)
                    );
                if (
                    !m_source.readAt(
                        m_dataOffset + m_compressedRead,
                        inputSize,
                        &m_inputBuffer
                        )
                    )
                {
                    fail(QStringLiteral("Could not read ZIP entry data."));
                    return -1;
                }

                m_stream.next_in = reinterpret_cast<Bytef*>(m_inputBuffer.data());
                m_stream.avail_in = static_cast<uInt>(inputSize);
                m_compressedRead += inputSize;
                m_compressedRemaining -= inputSize;
            }

            const quint32 outputRemaining =
                m_entry.uncompressedSize - m_uncompressedRead;
            if (outputRemaining == 0)
            {
                char overflowByte = 0;
                m_stream.next_out = reinterpret_cast<Bytef*>(&overflowByte);
                m_stream.avail_out = 1;
                const int status = inflate(&m_stream, Z_NO_FLUSH);
                if (status == Z_STREAM_END && m_stream.avail_out == 1)
                {
                    m_streamEnded = true;
                    break;
                }

                fail(QStringLiteral("ZIP entry exceeds its declared size."));
                return -1;
            }

            const quint32 outputSize = static_cast<quint32>(qMin<qint64>(
                qMin<qint64>(maximumSize - written, InputChunkSize),
                outputRemaining
                ));
            m_stream.next_out = reinterpret_cast<Bytef*>(destination + written);
            m_stream.avail_out = static_cast<uInt>(outputSize);
            const int status = inflate(&m_stream, Z_NO_FLUSH);
            const quint32 produced = outputSize - m_stream.avail_out;
            written += produced;
            m_uncompressedRead += produced;

            if (status == Z_STREAM_END)
            {
                m_streamEnded = true;
                if (m_uncompressedRead != m_entry.uncompressedSize)
                {
                    fail(QStringLiteral("ZIP entry ended before its declared size."));
                    return -1;
                }
            }
            else if (status != Z_OK)
            {
                fail(QStringLiteral("Could not decompress ZIP entry."));
                return -1;
            }
            else if (
                produced == 0
                && m_stream.avail_in == 0
                && m_compressedRemaining == 0
                )
            {
                fail(QStringLiteral("Truncated ZIP entry."));
                return -1;
            }
        }

        return written;
    }

    qint64 writeData(const char*, qint64) override
    {
        return -1;
    }

private:
    static constexpr qint64 InputChunkSize = 32 * 1024;

    void fail(QString message)
    {
        m_failed = true;
        setErrorString(std::move(message));
    }

    const ZipArchiveSource& m_source;
    ZipEntry m_entry;
    qint64 m_dataOffset = 0;
    quint32 m_compressedRead = 0;
    quint32 m_compressedRemaining = 0;
    quint32 m_uncompressedRead = 0;
    QByteArray m_inputBuffer;
    z_stream m_stream{};
    bool m_inflateInitialized = false;
    bool m_streamEnded = false;
    bool m_valid = false;
    bool m_failed = false;
};

class ZipArchiveReadSession final : public ArchiveReadSession
{
public:
    explicit ZipArchiveReadSession(const QByteArray& data)
        : m_source(data)
        , m_entries(zipEntries(m_source))
    {
    }

    explicit ZipArchiveReadSession(QIODevice& device)
        : m_source(device)
        , m_entries(zipEntries(m_source))
    {
    }

    [[nodiscard]] bool isValid() const noexcept override
    {
        return m_source.isValid();
    }

    [[nodiscard]] QStringList entryNames() const override
    {
        QStringList names;
        names.reserve(m_entries.size());
        for (auto iterator = m_entries.cbegin();
             iterator != m_entries.cend();
             ++iterator)
        {
            names.append(iterator.key());
        }
        return names;
    }

    [[nodiscard]] bool contains(const QString& entryName) const override
    {
        return m_entries.contains(entryName);
    }

    [[nodiscard]] QByteArray readEntry(const QString& entryName) override
    {
        std::unique_ptr<ArchiveEntryReadDevice> device = openEntry(entryName);
        if (!device || !device->isValid())
        {
            return {};
        }

        QByteArray data;
        while (true)
        {
            const QByteArray chunk = device->read(32 * 1024);
            if (chunk.isEmpty())
            {
                break;
            }
            data.append(chunk);
        }

        return device->failed() ? QByteArray{} : data;
    }

    [[nodiscard]] std::unique_ptr<ArchiveEntryReadDevice> openEntry(
        const QString& entryName
        ) override
    {
        const auto iterator = m_entries.constFind(entryName);
        if (iterator == m_entries.cend())
        {
            return {};
        }

        return std::make_unique<ZipArchiveEntryReadDevice>(
            m_source,
            iterator.value()
            );
    }

    [[nodiscard]] bool failed() const noexcept override
    {
        return m_source.failed();
    }

private:
    ZipArchiveSource m_source;
    QHash<QString, ZipEntry> m_entries;
};

class ZipArchiveReadPort final : public ArchiveReadPort
{
public:
    [[nodiscard]] std::unique_ptr<ArchiveReadSession> open(
        const QByteArray& data
        ) const override
    {
        return std::make_unique<ZipArchiveReadSession>(data);
    }

    [[nodiscard]] std::unique_ptr<ArchiveReadSession> open(
        QIODevice& device
        ) const override
    {
        return std::make_unique<ZipArchiveReadSession>(device);
    }
};
}

std::unique_ptr<ArchiveReadPort> makeZipArchiveReadPort()
{
    return std::make_unique<ZipArchiveReadPort>();
}

#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QStringList>

#include <memory>

class ArchiveEntryReadDevice : public QIODevice
{
public:
    ~ArchiveEntryReadDevice() override = default;

    [[nodiscard]] virtual bool isValid() const noexcept = 0;
    [[nodiscard]] virtual bool failed() const noexcept = 0;
};

// A synchronous view of one archive. The session must outlive every entry
// device it returns. The input source remains caller-owned and must outlive
// the session and its entry devices.
class ArchiveReadSession
{
public:
    virtual ~ArchiveReadSession() = default;

    [[nodiscard]] virtual bool isValid() const noexcept = 0;
    [[nodiscard]] virtual QStringList entryNames() const = 0;
    [[nodiscard]] virtual bool contains(const QString& entryName) const = 0;
    [[nodiscard]] virtual QByteArray readEntry(const QString& entryName) = 0;
    [[nodiscard]] virtual std::unique_ptr<ArchiveEntryReadDevice> openEntry(
        const QString& entryName
        ) = 0;
    [[nodiscard]] virtual bool failed() const noexcept = 0;
};

// Archive implementations hide the package format and compression library
// from readers. A session and its streams remain scoped to the input's
// lifetime; device inputs are borrowed and never closed by the port.
class ArchiveReadPort
{
public:
    virtual ~ArchiveReadPort() = default;

    [[nodiscard]] virtual std::unique_ptr<ArchiveReadSession> open(
        const QByteArray& data
        ) const = 0;
    [[nodiscard]] virtual std::unique_ptr<ArchiveReadSession> open(
        QIODevice& device
        ) const = 0;
};

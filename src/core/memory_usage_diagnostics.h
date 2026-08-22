#pragma once

#include "process_memory_snapshot.h"

#include <QJsonDocument>
#include <QList>
#include <QString>

struct MemoryUsageDelta
{
    qint64 absoluteBytes = 0;
    double percentage = 0.0;
    bool percentageAvailable = false;
};

namespace MemoryUsageMetrics
{
[[nodiscard]] QString formatBytes(quint64 bytes);
[[nodiscard]] MemoryUsageDelta calculateDelta(
    quint64 current,
    quint64 baseline
    );
}

enum class MemoryUsageHistoryEntryKind
{
    Sample,
    Event
};

struct MemoryUsageHistoryEntry
{
    MemoryUsageHistoryEntryKind kind = MemoryUsageHistoryEntryKind::Sample;
    ProcessMemorySnapshot snapshot;
    QString eventType;
    QString eventDetail;
};

class MemoryUsageHistory
{
public:
    explicit MemoryUsageHistory(int capacity = 600);

    void setCapacity(int capacity);
    [[nodiscard]] int capacity() const;
    [[nodiscard]] int size() const;
    [[nodiscard]] const QList<MemoryUsageHistoryEntry>& entries() const;

    void clear();
    void addSnapshot(const ProcessMemorySnapshot& snapshot);
    void addEvent(
        const QString& type,
        const QString& detail = QString()
        );

    [[nodiscard]] QJsonDocument toJson() const;

private:
    static QString redactText(const QString& text);
    void trimToCapacity();

    int m_capacity = 600;
    QList<MemoryUsageHistoryEntry> m_entries;
};

// Event collection is inert until the developer opens the monitor. This keeps
// normal application use free of diagnostic-history allocations.
class MemoryUsageDiagnostics
{
public:
    static void enable();
    [[nodiscard]] static bool isEnabled();
    static void recordSnapshot(const ProcessMemorySnapshot& snapshot);
    static void recordEvent(
        const QString& type,
        const QString& detail = QString()
        );
    [[nodiscard]] static MemoryUsageHistory& history();
};

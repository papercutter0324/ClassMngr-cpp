#pragma once

#include "process_memory_snapshot.h"

#include <QJsonDocument>
#include <QList>
#include <QPointer>
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

struct DeveloperBackgroundTask
{
    quint64 id = 0;
    QString category;
    QString name;
    QDateTime startedAt;
    bool cancellable = false;
    bool cancellationRequested = false;
};

// Feature-owned retained data only. These are intentionally partial,
// low-overhead estimates and must never be presented as process allocation
// accounting.
struct MemoryBreakdownEntry
{
    QString category;
    QString owner;
    quint64 retainedBytes = 0;
    quint64 itemCount = 0;
    QString detail;
    bool isEstimated = false;
};

class MemoryBreakdownProvider
{
public:
    virtual ~MemoryBreakdownProvider() = default;

    [[nodiscard]] virtual QList<MemoryBreakdownEntry>
        memoryBreakdown() const = 0;
};

enum class PageLifecycleState
{
    Uncreated,
    Hidden,
    Current
};

struct PageLifecycleEntry
{
    QString pageIdentifier;
    PageLifecycleState state = PageLifecycleState::Uncreated;
    QDateTime createdAt;
    QDateTime lastActivatedAt;
};

class PageLifecycleProvider
{
public:
    virtual ~PageLifecycleProvider() = default;

    [[nodiscard]] virtual QList<PageLifecycleEntry>
        pageLifecycle() const = 0;
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
    [[nodiscard]] static QString redactText(const QString& text);

private:
    [[nodiscard]] static qsizetype entryByteEstimate(
        const MemoryUsageHistoryEntry& entry
        );
    void trimToCapacity();

    int m_capacity = 600;
    qsizetype m_entryBytes = 0;
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
    static void recordTimedOperation(
        const QString& category,
        const QString& detail,
        qint64 elapsedMilliseconds,
        qint64 slowThresholdMilliseconds = 500
        );
    [[nodiscard]] static quint64 beginBackgroundTask(
        const QString& category,
        const QString& name,
        bool cancellable = false
        );
    static void finishBackgroundTask(
        quint64 taskId,
        bool cancelled = false
        );
    [[nodiscard]] static QList<DeveloperBackgroundTask>
        activeBackgroundTasks();
    static void registerMemoryBreakdownProvider(
        QObject* owner,
        const MemoryBreakdownProvider* provider
        );
    static void registerPageLifecycleProvider(
        QObject* owner,
        const PageLifecycleProvider* provider
        );
    [[nodiscard]] static QList<MemoryBreakdownEntry>
        collectMemoryBreakdown();
    [[nodiscard]] static QList<PageLifecycleEntry>
        collectPageLifecycle();
    [[nodiscard]] static MemoryUsageHistory& history();
};

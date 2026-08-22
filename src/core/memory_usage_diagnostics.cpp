#include "memory_usage_diagnostics.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
bool s_enabled = false;
constexpr qsizetype MaximumHistoryBytes = 64 * 1024;
constexpr qsizetype MaximumTrackedBackgroundTasks = 64;
constexpr qsizetype MaximumEventTextCharacters = 4096;

struct MemoryBreakdownProviderRegistration
{
    QPointer<QObject> owner;
    const MemoryBreakdownProvider* provider = nullptr;
};

struct PageLifecycleProviderRegistration
{
    QPointer<QObject> owner;
    const PageLifecycleProvider* provider = nullptr;
};

QList<MemoryBreakdownProviderRegistration>& memoryBreakdownProviders()
{
    static QList<MemoryBreakdownProviderRegistration> providers;
    return providers;
}

QList<PageLifecycleProviderRegistration>& pageLifecycleProviders()
{
    static QList<PageLifecycleProviderRegistration> providers;
    return providers;
}

QList<DeveloperBackgroundTask>& activeBackgroundTasks()
{
    static QList<DeveloperBackgroundTask> tasks;
    return tasks;
}

quint64& nextBackgroundTaskId()
{
    static quint64 id = 1;
    return id;
}

MemoryUsageHistory& sharedHistory()
{
    static MemoryUsageHistory history;
    return history;
}

QJsonObject snapshotJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("capturedAt"), snapshot.capturedAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("workingSetBytes"), static_cast<double>(snapshot.workingSetBytes)},
        {QStringLiteral("peakWorkingSetBytes"), static_cast<double>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"), static_cast<double>(snapshot.privateUsageBytes)},
        {QStringLiteral("pagefileUsageBytes"), static_cast<double>(snapshot.pagefileUsageBytes)},
        {QStringLiteral("handleCount"), static_cast<int>(snapshot.handleCount)},
        {QStringLiteral("threadCount"), static_cast<int>(snapshot.threadCount)}
    };
}
}

QString MemoryUsageMetrics::formatBytes(quint64 bytes)
{
    static constexpr std::array<const char*, 5> Units{
        "B", "KiB", "MiB", "GiB", "TiB"
    };

    double value = static_cast<double>(bytes);
    int unit = 0;

    while (value >= 1024.0 && unit < static_cast<int>(Units.size()) - 1)
    {
        value /= 1024.0;
        ++unit;
    }

    const int decimals = unit == 0 ? 0 : (value < 10.0 ? 2 : 1);
    return QStringLiteral("%1 %2")
        .arg(QString::number(value, 'f', decimals), QString::fromLatin1(Units.at(unit)));
}

MemoryUsageDelta MemoryUsageMetrics::calculateDelta(
    quint64 current,
    quint64 baseline
    )
{
    const qint64 absolute = current >= baseline
        ? static_cast<qint64>(current - baseline)
        : -static_cast<qint64>(baseline - current);

    return {
        absolute,
        baseline == 0
            ? 0.0
            : (static_cast<double>(absolute) * 100.0 / static_cast<double>(baseline)),
        baseline != 0
    };
}

MemoryUsageHistory::MemoryUsageHistory(int capacity)
{
    setCapacity(capacity);
}

void MemoryUsageHistory::setCapacity(int capacity)
{
    m_capacity = std::max(1, capacity);
    trimToCapacity();
}

int MemoryUsageHistory::capacity() const
{
    return m_capacity;
}

int MemoryUsageHistory::size() const
{
    return m_entries.size();
}

const QList<MemoryUsageHistoryEntry>& MemoryUsageHistory::entries() const
{
    return m_entries;
}

void MemoryUsageHistory::clear()
{
    m_entries.clear();
    m_entryBytes = 0;
}

void MemoryUsageHistory::addSnapshot(const ProcessMemorySnapshot& snapshot)
{
    const MemoryUsageHistoryEntry entry{
        MemoryUsageHistoryEntryKind::Sample,
        snapshot,
        {},
        {}
    };
    m_entryBytes += entryByteEstimate(entry);
    m_entries.append(entry);
    trimToCapacity();
}

void MemoryUsageHistory::addEvent(
    const QString& type,
    const QString& detail
    )
{
    ProcessMemorySnapshot timestamp;
    timestamp.capturedAt = QDateTime::currentDateTime();
    const MemoryUsageHistoryEntry entry{
        MemoryUsageHistoryEntryKind::Event,
        timestamp,
        redactText(type),
        redactText(detail)
    };
    m_entryBytes += entryByteEstimate(entry);
    m_entries.append(entry);
    trimToCapacity();
}

QJsonDocument MemoryUsageHistory::toJson() const
{
    QJsonArray entries;

    for (const MemoryUsageHistoryEntry& entry : m_entries)
    {
        QJsonObject object;
        object.insert(
            QStringLiteral("kind"),
            entry.kind == MemoryUsageHistoryEntryKind::Sample
                ? QStringLiteral("sample")
                : QStringLiteral("event")
            );
        object.insert(
            QStringLiteral("capturedAt"),
            entry.snapshot.capturedAt.toString(Qt::ISODateWithMs)
            );

        if (entry.kind == MemoryUsageHistoryEntryKind::Sample)
        {
            object.insert(QStringLiteral("snapshot"), snapshotJson(entry.snapshot));
        }
        else
        {
            object.insert(QStringLiteral("eventType"), entry.eventType);
            object.insert(QStringLiteral("eventDetail"), entry.eventDetail);
        }

        entries.append(object);
    }

    return QJsonDocument(
        {
            {QStringLiteral("format"), QStringLiteral("classmngr-memory-diagnostics-v1")},
            {QStringLiteral("entries"), entries}
        }
        );
}

QString MemoryUsageHistory::redactText(const QString& text)
{
    QString redacted = text;
    static const QRegularExpression filePath(
        QStringLiteral(R"(([A-Za-z]:[\\/][^\s,;]+|(?:file:)?//[^\s,;]+|/[A-Za-z0-9._-]+(?:/[^\s,;]+)+))")
        );
    redacted.replace(filePath, QStringLiteral("[redacted path]"));
    if (redacted.size() > MaximumEventTextCharacters)
    {
        redacted.truncate(MaximumEventTextCharacters);
        redacted.append(QStringLiteral(" [truncated]"));
    }
    return redacted;
}

qsizetype MemoryUsageHistory::entryByteEstimate(
    const MemoryUsageHistoryEntry& entry
    )
{
    // The snapshot's fixed-width fields are small; reserve a conservative
    // amount so the event log remains bounded even when it has no text.
    return 128
        + (entry.eventType.size() + entry.eventDetail.size())
              * static_cast<qsizetype>(sizeof(QChar));
}

void MemoryUsageHistory::trimToCapacity()
{
    while (
        !m_entries.isEmpty()
        && (
            m_entries.size() > m_capacity
            || (m_entryBytes > MaximumHistoryBytes && m_entries.size() > 1)
            )
        )
    {
        m_entryBytes -= entryByteEstimate(m_entries.constFirst());
        m_entries.removeFirst();
    }
}

void MemoryUsageDiagnostics::enable()
{
    s_enabled = true;
}

bool MemoryUsageDiagnostics::isEnabled()
{
    return s_enabled;
}

void MemoryUsageDiagnostics::recordSnapshot(
    const ProcessMemorySnapshot& snapshot
    )
{
    if (s_enabled)
    {
        sharedHistory().addSnapshot(snapshot);
    }
}

void MemoryUsageDiagnostics::recordEvent(
    const QString& type,
    const QString& detail
    )
{
    if (s_enabled)
    {
        sharedHistory().addEvent(type, detail);
    }
}

void MemoryUsageDiagnostics::recordTimedOperation(
    const QString& category,
    const QString& detail,
    qint64 elapsedMilliseconds,
    qint64 slowThresholdMilliseconds
    )
{
    if (!s_enabled || elapsedMilliseconds < 0)
    {
        return;
    }

    const QString operationDetail =
        QStringLiteral("%1; elapsedMs=%2%3")
            .arg(
                category,
                QString::number(elapsedMilliseconds),
                detail.isEmpty()
                    ? QString()
                    : QStringLiteral("; %1").arg(detail)
                );
    recordEvent(
        slowThresholdMilliseconds > 0
            && elapsedMilliseconds >= slowThresholdMilliseconds
            ? QStringLiteral("slow-operation")
            : QStringLiteral("timing"),
        operationDetail
        );
}

quint64 MemoryUsageDiagnostics::beginBackgroundTask(
    const QString& category,
    const QString& name,
    bool cancellable
    )
{
    if (!s_enabled || ::activeBackgroundTasks().size() >= MaximumTrackedBackgroundTasks)
    {
        return 0;
    }

    quint64& nextId = nextBackgroundTaskId();
    const quint64 id = nextId++;
    ::activeBackgroundTasks().append(
        {
            id,
            MemoryUsageHistory::redactText(category),
            MemoryUsageHistory::redactText(name),
            QDateTime::currentDateTime(),
            cancellable,
            false
        }
        );
    recordEvent(
        QStringLiteral("background-task-started"),
        QStringLiteral("%1; %2").arg(category, name)
        );
    return id;
}

void MemoryUsageDiagnostics::finishBackgroundTask(
    quint64 taskId,
    bool cancelled
    )
{
    if (!s_enabled || taskId == 0)
    {
        return;
    }

    QList<DeveloperBackgroundTask>& tasks = ::activeBackgroundTasks();
    const auto iterator = std::find_if(
        tasks.cbegin(),
        tasks.cend(),
        [taskId](const DeveloperBackgroundTask& task)
        {
            return task.id == taskId;
        }
        );

    if (iterator == tasks.cend())
    {
        return;
    }

    const QString detail = QStringLiteral("%1; %2; %3")
        .arg(
            iterator->category,
            iterator->name,
            cancelled ? QStringLiteral("cancelled") : QStringLiteral("finished")
            );
    tasks.erase(iterator);
    recordEvent(QStringLiteral("background-task-finished"), detail);
}

QList<DeveloperBackgroundTask> MemoryUsageDiagnostics::activeBackgroundTasks()
{
    return ::activeBackgroundTasks();
}

void MemoryUsageDiagnostics::registerMemoryBreakdownProvider(
    QObject* owner,
    const MemoryBreakdownProvider* provider
    )
{
    if (!owner || !provider)
    {
        return;
    }

    QList<MemoryBreakdownProviderRegistration>& providers =
        memoryBreakdownProviders();
    for (const MemoryBreakdownProviderRegistration& registration : providers)
    {
        if (registration.owner == owner && registration.provider == provider)
        {
            return;
        }
    }

    providers.append({owner, provider});
}

void MemoryUsageDiagnostics::registerPageLifecycleProvider(
    QObject* owner,
    const PageLifecycleProvider* provider
    )
{
    if (!owner || !provider)
    {
        return;
    }

    QList<PageLifecycleProviderRegistration>& providers =
        pageLifecycleProviders();
    for (const PageLifecycleProviderRegistration& registration : providers)
    {
        if (registration.owner == owner && registration.provider == provider)
        {
            return;
        }
    }

    providers.append({owner, provider});
}

QList<MemoryBreakdownEntry> MemoryUsageDiagnostics::collectMemoryBreakdown()
{
    QList<MemoryBreakdownEntry> entries;
    QList<MemoryBreakdownProviderRegistration>& providers =
        memoryBreakdownProviders();

    for (auto iterator = providers.begin(); iterator != providers.end();)
    {
        if (!iterator->owner)
        {
            iterator = providers.erase(iterator);
            continue;
        }

        entries.append(iterator->provider->memoryBreakdown());
        ++iterator;
    }

    return entries;
}

QList<PageLifecycleEntry> MemoryUsageDiagnostics::collectPageLifecycle()
{
    QList<PageLifecycleEntry> entries;
    QList<PageLifecycleProviderRegistration>& providers =
        pageLifecycleProviders();

    for (auto iterator = providers.begin(); iterator != providers.end();)
    {
        if (!iterator->owner)
        {
            iterator = providers.erase(iterator);
            continue;
        }

        entries.append(iterator->provider->pageLifecycle());
        ++iterator;
    }

    return entries;
}

MemoryUsageHistory& MemoryUsageDiagnostics::history()
{
    return sharedHistory();
}

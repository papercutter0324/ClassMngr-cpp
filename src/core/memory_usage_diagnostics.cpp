#include "memory_usage_diagnostics.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
bool s_enabled = false;

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
}

void MemoryUsageHistory::addSnapshot(const ProcessMemorySnapshot& snapshot)
{
    m_entries.append({MemoryUsageHistoryEntryKind::Sample, snapshot, {}, {}});
    trimToCapacity();
}

void MemoryUsageHistory::addEvent(
    const QString& type,
    const QString& detail
    )
{
    ProcessMemorySnapshot timestamp;
    timestamp.capturedAt = QDateTime::currentDateTime();
    m_entries.append(
        {
            MemoryUsageHistoryEntryKind::Event,
            timestamp,
            redactText(type),
            redactText(detail)
        }
        );
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
    return redacted;
}

void MemoryUsageHistory::trimToCapacity()
{
    const int excess = m_entries.size() - m_capacity;

    if (excess > 0)
    {
        m_entries.remove(0, excess);
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

MemoryUsageHistory& MemoryUsageDiagnostics::history()
{
    return sharedHistory();
}

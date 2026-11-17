#include "core/process_memory_diagnostics.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonValue>

#include <atomic>
#include <cstdio>
#include <utility>

namespace
{
QJsonValue counter(bool available, quint64 value)
{
    return available ? QJsonValue(static_cast<qint64>(value))
                     : QJsonValue(QJsonValue::Null);
}

QJsonObject metrics(const ProcessMemorySnapshot& memory)
{
    QJsonObject result;
    const auto add = [&](const char* key, bool available, quint64 value) {
        result.insert(QString::fromLatin1(key), counter(available, value));
    };
    if (memory.platform == QStringLiteral("windows"))
    {
        add("working_set_bytes", memory.workingSetBytesAvailable, memory.workingSetBytes);
        add("peak_working_set_bytes", memory.peakWorkingSetBytesAvailable, memory.peakWorkingSetBytes);
        add("private_usage_bytes", memory.privateUsageBytesAvailable, memory.privateUsageBytes);
        add("pagefile_usage_bytes", memory.pagefileUsageBytesAvailable, memory.pagefileUsageBytes);
        add("private_working_set_bytes", memory.privateWorkingSetBytesAvailable, memory.privateWorkingSetBytes);
        add("handle_count", memory.handleCountAvailable, memory.handleCount);
        add("thread_count", memory.threadCountAvailable, memory.threadCount);
    }
    else if (memory.platform == QStringLiteral("macos"))
    {
        add("resident_bytes", memory.workingSetBytesAvailable, memory.workingSetBytes);
        add("peak_resident_bytes", memory.peakWorkingSetBytesAvailable, memory.peakWorkingSetBytes);
        add("physical_footprint_bytes", memory.privateUsageBytesAvailable, memory.privateUsageBytes);
        add("compressed_bytes", memory.pagefileUsageBytesAvailable, memory.pagefileUsageBytes);
        add("internal_bytes", memory.privateWorkingSetBytesAvailable, memory.privateWorkingSetBytes);
        add("thread_count", memory.threadCountAvailable, memory.threadCount);
    }
    else if (memory.platform == QStringLiteral("linux"))
    {
        add("rss_bytes", memory.workingSetBytesAvailable, memory.workingSetBytes);
        add("peak_rss_bytes", memory.peakWorkingSetBytesAvailable, memory.peakWorkingSetBytes);
        add("pss_bytes", memory.privateUsageBytesAvailable, memory.privateUsageBytes);
        add("private_clean_and_dirty_bytes", memory.pagefileUsageBytesAvailable, memory.pagefileUsageBytes);
        add("anonymous_resident_bytes", memory.privateWorkingSetBytesAvailable, memory.privateWorkingSetBytes);
        add("private_dirty_bytes", memory.privateDirtyBytesAvailable, memory.privateDirtyBytes);
        add("file_descriptor_count", memory.handleCountAvailable, memory.handleCount);
        add("thread_count", memory.threadCountAvailable, memory.threadCount);
    }
    return result;
}
}

QJsonObject processMemoryDiagnosticObject(
    const QString& event, const QString& owner, quint64 instanceId,
    const QJsonObject& details, const ProcessMemorySnapshot& memory)
{
    return {
        {QStringLiteral("event"), event},
        {QStringLiteral("owner"), owner},
        {QStringLiteral("instance_id"), static_cast<qint64>(instanceId)},
        {QStringLiteral("details"), details},
        {QStringLiteral("captured_at"), memory.capturedAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("platform"), memory.platform},
        {QStringLiteral("sample_available"), memory.isAvailable},
        {QStringLiteral("memory_scope"), QStringLiteral("whole_process")},
        {QStringLiteral("memory"), metrics(memory)}
    };
}

quint64 nextProcessMemoryDiagnosticId() noexcept
{
    static std::atomic<quint64> next{0};
    return next.fetch_add(1, std::memory_order_relaxed) + 1;
}

ProcessMemoryDiagnostics::ProcessMemoryDiagnostics(
    std::unique_ptr<ProcessMemorySnapshotProvider> provider, Writer writer)
    : m_provider(provider ? std::move(provider)
                          : std::make_unique<PlatformProcessMemorySnapshotProvider>())
    , m_writer(writer ? std::move(writer) : Writer([](const QByteArray& record) {
        qInfo().noquote().nospace() << "ClassMngr lifecycle checkpoint=" << record;
      }))
{
}

void ProcessMemoryDiagnostics::record(
    const QString& event, const QString& owner, quint64 instanceId,
    const QJsonObject& details) noexcept
{
    try
    {
        const std::lock_guard lock(m_mutex);
        const auto memory = m_provider->snapshot();
        m_writer(QJsonDocument(processMemoryDiagnosticObject(
            event, owner, instanceId, details, memory)).toJson(QJsonDocument::Compact));
    }
    catch (...)
    {
        std::fputs("ClassMngr lifecycle diagnostics failed.\n", stderr);
    }
}

void recordProcessMemoryDiagnostic(
    const QString& event, const QString& owner, quint64 instanceId,
    const QJsonObject& details) noexcept
{
    try
    {
        static ProcessMemoryDiagnostics diagnostics;
        diagnostics.record(event, owner, instanceId, details);
    }
    catch (...)
    {
        std::fputs("ClassMngr lifecycle diagnostics unavailable.\n", stderr);
    }
}

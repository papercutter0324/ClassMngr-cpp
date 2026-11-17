#pragma once

#include "core/process_memory_snapshot.h"

#include <QByteArray>
#include <QJsonObject>

#include <functional>
#include <memory>
#include <mutex>

// QtCore-only formatting shared by lifecycle owners. Details must contain
// counters and opaque identities only, never user paths or document titles.
[[nodiscard]] QJsonObject processMemoryDiagnosticObject(
    const QString& event, const QString& owner, quint64 instanceId,
    const QJsonObject& details, const ProcessMemorySnapshot& memory);
[[nodiscard]] quint64 nextProcessMemoryDiagnosticId() noexcept;

class ProcessMemoryDiagnostics final
{
public:
    using Writer = std::function<void(const QByteArray&)>;

    explicit ProcessMemoryDiagnostics(
        std::unique_ptr<ProcessMemorySnapshotProvider> provider = {},
        Writer writer = {});
    void record(const QString& event, const QString& owner, quint64 instanceId,
                const QJsonObject& details = {}) noexcept;

private:
    std::unique_ptr<ProcessMemorySnapshotProvider> m_provider;
    Writer m_writer;
    std::mutex m_mutex;
};

// Shared default recorder retains no event history or application objects.
void recordProcessMemoryDiagnostic(
    const QString& event, const QString& owner, quint64 instanceId,
    const QJsonObject& details = {}) noexcept;

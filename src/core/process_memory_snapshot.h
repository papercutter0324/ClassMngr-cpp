#pragma once

#include <QDateTime>

// A small, platform-specific process-memory sample. On Windows the values map
// directly to PROCESS_MEMORY_COUNTERS_EX fields; unsupported platforms return
// an unavailable sample instead of guessing at equivalent metrics.
struct ProcessMemorySnapshot
{
    bool isAvailable = false;
    // Windows: working set. macOS: resident memory. Linux: RSS.
    quint64 workingSetBytes = 0;
    // Windows: peak working set. macOS: peak resident memory. Linux: VmHWM.
    quint64 peakWorkingSetBytes = 0;
    // Windows: private usage. macOS: physical footprint. Linux: PSS.
    quint64 privateUsageBytes = 0;
    // Windows: pagefile usage. Linux: private dirty memory where available.
    quint64 pagefileUsageBytes = 0;
    quint32 handleCount = 0;
    quint32 threadCount = 0;
    QDateTime capturedAt;

    // Available on platforms where a more specific private resident value can
    // be obtained. Windows uses PrivateWorkingSetSize; Linux uses RssAnon.
    quint64 privateWorkingSetBytes = 0;
    quint64 privateDirtyBytes = 0;
    QString platform;
};

class ProcessMemorySnapshotProvider
{
public:
    virtual ~ProcessMemorySnapshotProvider() = default;

    [[nodiscard]] virtual ProcessMemorySnapshot snapshot() const = 0;
};

class PlatformProcessMemorySnapshotProvider final
    : public ProcessMemorySnapshotProvider
{
public:
    [[nodiscard]] ProcessMemorySnapshot snapshot() const override;
};

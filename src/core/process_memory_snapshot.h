#pragma once

#include <QDateTime>

// A small, platform-specific process-memory sample. On Windows the values map
// directly to PROCESS_MEMORY_COUNTERS_EX fields; unsupported platforms return
// an unavailable sample instead of guessing at equivalent metrics.
struct ProcessMemorySnapshot
{
    bool isAvailable = false;
    quint64 workingSetBytes = 0;       // Windows: WorkingSetSize
    quint64 peakWorkingSetBytes = 0;   // Windows: PeakWorkingSetSize
    quint64 privateUsageBytes = 0;     // Windows: PrivateUsage
    quint64 pagefileUsageBytes = 0;    // Windows: PagefileUsage
    quint32 handleCount = 0;
    quint32 threadCount = 0;
    QDateTime capturedAt;
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

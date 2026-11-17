#include "process_memory_snapshot.h"

#include <limits>
#include <optional>
#include <utility>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#elif defined(Q_OS_MACOS)
#include <mach/mach.h>
#include <mach/task_info.h>
#elif defined(Q_OS_LINUX)
#include <QDir>
#include <QFile>
#endif

namespace
{
#if defined(Q_OS_LINUX)
std::optional<quint64> procKiBValue(
    const QString& filePath,
    const QByteArray& key
    )
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return std::nullopt;
    }

    // Some procfs pseudo-files report a size of zero even though they still
    // produce data. QFile::atEnd() checks that size and can therefore be true
    // before the first read. Read until readLine() returns no bytes instead.
    while (true)
    {
        const QByteArray line = file.readLine();
        if (line.isEmpty())
        {
            break;
        }

        if (!line.startsWith(key))
        {
            continue;
        }

        const QList<QByteArray> parts =
            line.mid(key.size()).simplified().split(' ');
        if (parts.isEmpty())
        {
            return std::nullopt;
        }

        bool converted = false;
        const quint64 kibibytes = parts.constFirst().toULongLong(&converted);
        return converted
            ? std::optional<quint64>(kibibytes * 1024)
            : std::nullopt;
    }

    return std::nullopt;
}

std::optional<quint64> procEntryCount(const QString& path)
{
    QDir directory(path);
    if (!directory.exists() || !directory.isReadable())
    {
        return std::nullopt;
    }

    return static_cast<quint64>(
        directory.entryList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
            ).size()
        );
}
#endif
}

#if defined(Q_OS_LINUX)
PlatformProcessMemorySnapshotProvider
    ::PlatformProcessMemorySnapshotProvider(QString procRoot)
    : m_procRoot(std::move(procRoot))
{
}
#endif

ProcessMemorySnapshot PlatformProcessMemorySnapshotProvider::snapshot() const
{
    ProcessMemorySnapshot result;
    result.capturedAt = QDateTime::currentDateTime();

#if defined(Q_OS_WIN)
    result.platform = QStringLiteral("windows");

    PROCESS_MEMORY_COUNTERS_EX2 counters{};
    counters.cb = sizeof(counters);

    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)
            ))
    {
        return result;
    }

    result.isAvailable = true;
    result.workingSetBytes = counters.WorkingSetSize;
    result.workingSetBytesAvailable = true;
    result.peakWorkingSetBytes = counters.PeakWorkingSetSize;
    result.peakWorkingSetBytesAvailable = true;
    result.privateUsageBytes = counters.PrivateUsage;
    result.privateUsageBytesAvailable = true;
    result.pagefileUsageBytes = counters.PagefileUsage;
    result.pagefileUsageBytesAvailable = true;
    result.privateWorkingSetBytes = counters.PrivateWorkingSetSize;
    result.privateWorkingSetBytesAvailable = true;

    DWORD handleCount = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handleCount))
    {
        result.handleCount = handleCount;
        result.handleCountAvailable = true;
    }

    const DWORD processId = GetCurrentProcessId();
    const HANDLE threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (threadSnapshot != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 thread{};
        thread.dwSize = sizeof(thread);

        if (Thread32First(threadSnapshot, &thread))
        {
            while (true)
            {
                if (thread.th32OwnerProcessID == processId)
                {
                    ++result.threadCount;
                }

                SetLastError(ERROR_SUCCESS);
                if (!Thread32Next(threadSnapshot, &thread))
                {
                    result.threadCountAvailable =
                        GetLastError() == ERROR_NO_MORE_FILES;
                    break;
                }
            }
        }

        CloseHandle(threadSnapshot);
    }
#elif defined(Q_OS_MACOS)
    result.platform = QStringLiteral("macos");

    task_vm_info_data_t vmInfo{};
    mach_msg_type_number_t vmInfoCount = TASK_VM_INFO_COUNT;
    if (task_info(
            mach_task_self(),
            TASK_VM_INFO,
            reinterpret_cast<task_info_t>(&vmInfo),
            &vmInfoCount
            ) != KERN_SUCCESS)
    {
        return result;
    }

    result.isAvailable = true;
    result.workingSetBytes = vmInfo.resident_size;
    result.workingSetBytesAvailable = true;
    result.peakWorkingSetBytes = vmInfo.resident_size_peak;
    result.peakWorkingSetBytesAvailable = true;
    result.privateUsageBytes = vmInfo.phys_footprint;
    result.privateUsageBytesAvailable = true;
    result.privateWorkingSetBytes = vmInfo.internal;
    result.privateWorkingSetBytesAvailable = true;
    result.pagefileUsageBytes = vmInfo.compressed;
    result.pagefileUsageBytesAvailable = true;

    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t threadCount = 0;
    if (task_threads(mach_task_self(), &threads, &threadCount) == KERN_SUCCESS)
    {
        result.threadCount = threadCount;
        result.threadCountAvailable = true;
        vm_deallocate(
            mach_task_self(),
            reinterpret_cast<vm_address_t>(threads),
            static_cast<vm_size_t>(threadCount * sizeof(thread_t))
            );
    }
#elif defined(Q_OS_LINUX)
    result.platform = QStringLiteral("linux");
    const QDir procDirectory(m_procRoot);
    const QString statusPath =
        procDirectory.filePath(QStringLiteral("self/status"));
    const QString smapsRollupPath =
        procDirectory.filePath(QStringLiteral("self/smaps_rollup"));
    const QString fileDescriptorsPath =
        procDirectory.filePath(QStringLiteral("self/fd"));
    const QString tasksPath =
        procDirectory.filePath(QStringLiteral("self/task"));

    const std::optional<quint64> rss = procKiBValue(
        statusPath,
        QByteArrayLiteral("VmRSS:")
        );
    if (!rss.has_value())
    {
        return result;
    }

    result.isAvailable = true;
    result.workingSetBytes = *rss;
    result.workingSetBytesAvailable = true;
    const std::optional<quint64> peakRss = procKiBValue(
        statusPath,
        QByteArrayLiteral("VmHWM:")
        );
    if (peakRss.has_value())
    {
        result.peakWorkingSetBytes = *peakRss;
        result.peakWorkingSetBytesAvailable = true;
    }

    const std::optional<quint64> anonymousResident = procKiBValue(
        statusPath,
        QByteArrayLiteral("RssAnon:")
        );
    if (anonymousResident.has_value())
    {
        result.privateWorkingSetBytes = *anonymousResident;
        result.privateWorkingSetBytesAvailable = true;
    }

    const std::optional<quint64> proportionalSetSize = procKiBValue(
        smapsRollupPath,
        QByteArrayLiteral("Pss:")
        );
    if (proportionalSetSize.has_value())
    {
        result.privateUsageBytes = *proportionalSetSize;
        result.privateUsageBytesAvailable = true;
    }

    const std::optional<quint64> privateClean = procKiBValue(
        smapsRollupPath,
        QByteArrayLiteral("Private_Clean:")
        );
    const std::optional<quint64> privateDirty = procKiBValue(
        smapsRollupPath,
        QByteArrayLiteral("Private_Dirty:")
        );
    if (privateDirty.has_value())
    {
        result.privateDirtyBytes = *privateDirty;
        result.privateDirtyBytesAvailable = true;
    }
    if (privateClean.has_value() && privateDirty.has_value())
    {
        result.pagefileUsageBytes = *privateClean + *privateDirty;
        result.pagefileUsageBytesAvailable = true;
    }

    const std::optional<quint64> handleCount =
        procEntryCount(fileDescriptorsPath);
    if (handleCount.has_value())
    {
        result.handleCount = static_cast<quint32>(qMin<quint64>(
            *handleCount,
            std::numeric_limits<quint32>::max()
            ));
        result.handleCountAvailable = true;
    }
    const std::optional<quint64> threadCount = procEntryCount(tasksPath);
    if (threadCount.has_value())
    {
        result.threadCount = static_cast<quint32>(qMin<quint64>(
            *threadCount,
            std::numeric_limits<quint32>::max()
            ));
        result.threadCountAvailable = true;
    }
#endif

    return result;
}

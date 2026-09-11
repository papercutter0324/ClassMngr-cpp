#include "process_memory_snapshot.h"

#include <limits>

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
quint64 procKiBValue(
    const QString& filePath,
    const QByteArray& key
    )
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return 0;
    }

    // procfs files report a zero size even though they provide readable
    // content. QFile::atEnd() therefore reports EOF before the first read on
    // some Linux systems; use the null QByteArray returned by readLine() as
    // the actual EOF signal instead.
    while (true)
    {
        const QByteArray line = file.readLine();
        if (line.isNull())
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
            return 0;
        }

        bool converted = false;
        const quint64 kibibytes = parts.constFirst().toULongLong(&converted);
        return converted ? kibibytes * 1024 : 0;
    }

    return 0;
}

quint64 procEntryCount(const QString& path)
{
    QDir directory(path);
    return directory.exists()
        ? static_cast<quint64>(
            directory.entryList(
                QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                ).size()
            )
        : 0;
}
#endif
}

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
    result.peakWorkingSetBytes = counters.PeakWorkingSetSize;
    result.privateUsageBytes = counters.PrivateUsage;
    result.pagefileUsageBytes = counters.PagefileUsage;
    result.privateWorkingSetBytes = counters.PrivateWorkingSetSize;

    DWORD handleCount = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handleCount))
    {
        result.handleCount = handleCount;
    }

    const DWORD processId = GetCurrentProcessId();
    const HANDLE threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (threadSnapshot != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 thread{};
        thread.dwSize = sizeof(thread);

        if (Thread32First(threadSnapshot, &thread))
        {
            do
            {
                if (thread.th32OwnerProcessID == processId)
                {
                    ++result.threadCount;
                }
            }
            while (Thread32Next(threadSnapshot, &thread));
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
    result.peakWorkingSetBytes = vmInfo.resident_size_peak;
    result.privateUsageBytes = vmInfo.phys_footprint;
    result.privateWorkingSetBytes = vmInfo.internal;
    result.pagefileUsageBytes = vmInfo.compressed;

    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t threadCount = 0;
    if (task_threads(mach_task_self(), &threads, &threadCount) == KERN_SUCCESS)
    {
        result.threadCount = threadCount;
        vm_deallocate(
            mach_task_self(),
            reinterpret_cast<vm_address_t>(threads),
            static_cast<vm_size_t>(threadCount * sizeof(thread_t))
            );
    }
#elif defined(Q_OS_LINUX)
    result.platform = QStringLiteral("linux");

    const quint64 rss = procKiBValue(
        QStringLiteral("/proc/self/status"),
        QByteArrayLiteral("VmRSS:")
        );
    if (rss == 0)
    {
        return result;
    }

    result.isAvailable = true;
    result.workingSetBytes = rss;
    result.peakWorkingSetBytes = procKiBValue(
        QStringLiteral("/proc/self/status"),
        QByteArrayLiteral("VmHWM:")
        );
    result.privateWorkingSetBytes = procKiBValue(
        QStringLiteral("/proc/self/status"),
        QByteArrayLiteral("RssAnon:")
        );
    result.privateUsageBytes = procKiBValue(
        QStringLiteral("/proc/self/smaps_rollup"),
        QByteArrayLiteral("Pss:")
        );
    if (result.privateUsageBytes == 0)
    {
        result.privateUsageBytes = result.privateWorkingSetBytes;
    }

    const quint64 privateClean = procKiBValue(
        QStringLiteral("/proc/self/smaps_rollup"),
        QByteArrayLiteral("Private_Clean:")
        );
    const quint64 privateDirty = procKiBValue(
        QStringLiteral("/proc/self/smaps_rollup"),
        QByteArrayLiteral("Private_Dirty:")
        );
    result.privateDirtyBytes = privateDirty;
    result.pagefileUsageBytes = privateClean + privateDirty;
    result.handleCount = static_cast<quint32>(qMin<quint64>(
        procEntryCount(QStringLiteral("/proc/self/fd")),
        std::numeric_limits<quint32>::max()
        ));
    result.threadCount = static_cast<quint32>(qMin<quint64>(
        procEntryCount(QStringLiteral("/proc/self/task")),
        std::numeric_limits<quint32>::max()
        ));
#endif

    return result;
}

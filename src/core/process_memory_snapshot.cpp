#include "process_memory_snapshot.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#endif

ProcessMemorySnapshot PlatformProcessMemorySnapshotProvider::snapshot() const
{
    ProcessMemorySnapshot result;
    result.capturedAt = QDateTime::currentDateTime();

#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX counters{};
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
#endif

    return result;
}

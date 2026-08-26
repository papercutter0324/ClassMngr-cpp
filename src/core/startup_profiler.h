#pragma once

#include "process_memory_snapshot.h"

#include <QElapsedTimer>
#include <QList>
#include <QJsonObject>
#include <QString>

#include <functional>

// Application-level metrics intentionally describe the shared startup
// lifecycle, rather than any operating-system-specific implementation.
struct StartupApplicationMetrics
{
    int widgetCount = 0;
    int instantiatedPageCount = 0;
    int registeredPageCount = 0;
    int liveScheduleWidgetCount = 0;
    quint64 scheduleWidgetsCreated = 0;
    quint64 scheduleRenderCount = 0;
    quint64 scheduleTableItemsCreated = 0;
    quint64 scheduleCellWidgetsCreated = 0;
    quint64 scheduleCellWidgetsRemoved = 0;
    quint64 scheduleCellWidgetsQueuedForDeletion = 0;
};

struct StartupCheckpoint
{
    int sequence = 0;
    QString name;
    QString detail;
    qint64 elapsedMilliseconds = 0;
    ProcessMemorySnapshot memory;
    StartupApplicationMetrics metrics;
};

struct StartupProfilingEvent
{
    QString name;
    QString detail;
    qint64 elapsedMilliseconds = 0;
};

// The profiler is enabled only by an explicit startup profiling run.  Its
// static hooks therefore let UI code contribute structural diagnostics
// without allocating profiling data during normal application use.
class StartupProfiler
{
public:
    StartupProfiler();
    ~StartupProfiler();

    StartupProfiler(const StartupProfiler&) = delete;
    StartupProfiler& operator=(const StartupProfiler&) = delete;

    static void activate(StartupProfiler* profiler);
    [[nodiscard]] static bool isActive();
    static void setActiveApplicationMetricsProvider(
        std::function<StartupApplicationMetrics()> provider
        );

    void setApplicationMetricsProvider(
        std::function<StartupApplicationMetrics()> provider
        );
    void checkpoint(
        const QString& name,
        const QString& detail = QString()
        );

    [[nodiscard]] const QList<StartupCheckpoint>& checkpoints() const;
    [[nodiscard]] const QList<StartupProfilingEvent>& events() const;
    [[nodiscard]] QJsonObject reportJson() const;

    static void recordPageInstantiated(const QString& pageIdentifier);
    static void recordScheduleWidgetCreated(const QString& owner);
    static void recordScheduleWidgetDestroyed();
    static void recordStartupCompleteScheduleWidgetDiagnostic();
    static void recordScheduleRenderStarted(const QString& owner);
    static void recordScheduleRenderCompleted(
        const QString& owner,
        qint64 elapsedMilliseconds,
        int tableItemsCreated,
        int cellWidgetsCreated,
        int cellWidgetsRemoved,
        int cellWidgetsQueuedForDeletion
        );

private:
    void recordEvent(
        const QString& name,
        const QString& detail = QString()
        );
    [[nodiscard]] StartupApplicationMetrics applicationMetrics() const;

    QElapsedTimer m_timer;
    ProcessMemorySnapshotProvider* m_memoryProvider = nullptr;
    std::function<StartupApplicationMetrics()> m_applicationMetricsProvider;
    QList<StartupCheckpoint> m_checkpoints;
    QList<StartupProfilingEvent> m_events;
    StartupApplicationMetrics m_scheduleMetrics;
    PlatformProcessMemorySnapshotProvider m_platformMemoryProvider;
};

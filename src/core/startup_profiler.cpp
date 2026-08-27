#include "startup_profiler.h"

#include <QApplication>
#include <QJsonArray>

#include <utility>

namespace
{
StartupProfiler*& activeProfiler()
{
    static StartupProfiler* profiler = nullptr;
    return profiler;
}

QJsonObject memoryJson(const ProcessMemorySnapshot& memory)
{
    return {
        {QStringLiteral("available"), memory.isAvailable},
        {QStringLiteral("platform"), memory.platform},
        {QStringLiteral("capturedAt"), memory.capturedAt.toString(Qt::ISODateWithMs)},
        {QStringLiteral("workingSetBytes"), static_cast<double>(memory.workingSetBytes)},
        {QStringLiteral("peakWorkingSetBytes"), static_cast<double>(memory.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"), static_cast<double>(memory.privateUsageBytes)},
        {QStringLiteral("privateWorkingSetBytes"), static_cast<double>(memory.privateWorkingSetBytes)},
        {QStringLiteral("privateDirtyBytes"), static_cast<double>(memory.privateDirtyBytes)},
        {QStringLiteral("pagefileUsageBytes"), static_cast<double>(memory.pagefileUsageBytes)},
        {QStringLiteral("handleCount"), static_cast<int>(memory.handleCount)},
        {QStringLiteral("threadCount"), static_cast<int>(memory.threadCount)}
    };
}

QJsonObject applicationMetricsJson(const StartupApplicationMetrics& metrics)
{
    return {
        {QStringLiteral("widgetCount"), metrics.widgetCount},
        {QStringLiteral("instantiatedPageCount"), metrics.instantiatedPageCount},
        {QStringLiteral("registeredPageCount"), metrics.registeredPageCount},
        {QStringLiteral("liveScheduleWidgetCount"), metrics.liveScheduleWidgetCount},
        {QStringLiteral("scheduleWidgetsCreated"), static_cast<double>(metrics.scheduleWidgetsCreated)},
        {QStringLiteral("scheduleRenderCount"), static_cast<double>(metrics.scheduleRenderCount)},
        {QStringLiteral("scheduleTableItemsCreated"), static_cast<double>(metrics.scheduleTableItemsCreated)},
        {QStringLiteral("scheduleCellWidgetsCreated"), static_cast<double>(metrics.scheduleCellWidgetsCreated)},
        {QStringLiteral("scheduleCellWidgetsRemoved"), static_cast<double>(metrics.scheduleCellWidgetsRemoved)},
        {
            QStringLiteral("scheduleCellWidgetsQueuedForDeletion"),
            static_cast<double>(metrics.scheduleCellWidgetsQueuedForDeletion)
        }
    };
}
}

StartupProfiler::StartupProfiler()
    : m_memoryProvider(&m_platformMemoryProvider)
{
    m_timer.start();
}

StartupProfiler::~StartupProfiler()
{
    if (activeProfiler() == this)
    {
        activeProfiler() = nullptr;
    }
}

void StartupProfiler::activate(StartupProfiler* profiler)
{
    activeProfiler() = profiler;
}

bool StartupProfiler::isActive()
{
    return activeProfiler() != nullptr;
}

void StartupProfiler::setActiveApplicationMetricsProvider(
    std::function<StartupApplicationMetrics()> provider
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->setApplicationMetricsProvider(std::move(provider));
    }
}

void StartupProfiler::setApplicationMetricsProvider(
    std::function<StartupApplicationMetrics()> provider
    )
{
    m_applicationMetricsProvider = std::move(provider);
}

void StartupProfiler::checkpoint(
    const QString& name,
    const QString& detail
    )
{
    const ProcessMemorySnapshot memory =
        m_memoryProvider
            ? m_memoryProvider->snapshot()
            : ProcessMemorySnapshot{};

    m_checkpoints.append(
        {
            static_cast<int>(m_checkpoints.size()) + 1,
            name,
            detail,
            m_timer.elapsed(),
            memory,
            applicationMetrics()
        }
        );
}

const QList<StartupCheckpoint>& StartupProfiler::checkpoints() const
{
    return m_checkpoints;
}

const QList<StartupProfilingEvent>& StartupProfiler::events() const
{
    return m_events;
}

QJsonObject StartupProfiler::reportJson() const
{
    QJsonArray checkpoints;
    for (const StartupCheckpoint& checkpoint : m_checkpoints)
    {
        checkpoints.append(
            QJsonObject{
                {QStringLiteral("sequence"), checkpoint.sequence},
                {QStringLiteral("name"), checkpoint.name},
                {QStringLiteral("detail"), checkpoint.detail},
                {QStringLiteral("elapsedMs"), static_cast<double>(checkpoint.elapsedMilliseconds)},
                {QStringLiteral("memory"), memoryJson(checkpoint.memory)},
                {QStringLiteral("metrics"), applicationMetricsJson(checkpoint.metrics)}
            }
            );
    }

    QJsonArray events;
    for (const StartupProfilingEvent& event : m_events)
    {
        events.append(
            QJsonObject{
                {QStringLiteral("name"), event.name},
                {QStringLiteral("detail"), event.detail},
                {QStringLiteral("elapsedMs"), static_cast<double>(event.elapsedMilliseconds)}
            }
            );
    }

    return {
        {QStringLiteral("checkpoints"), checkpoints},
        {QStringLiteral("events"), events}
    };
}

void StartupProfiler::recordPageInstantiated(const QString& pageIdentifier)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->recordEvent(
            QStringLiteral("page-created"),
            pageIdentifier
            );
    }
}

void StartupProfiler::recordScheduleWidgetCreated(const QString& owner)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.liveScheduleWidgetCount;
        ++profiler->m_scheduleMetrics.scheduleWidgetsCreated;
        profiler->recordEvent(
            QStringLiteral("schedule-widget-created"),
            owner
            );
    }
}

void StartupProfiler::recordScheduleWidgetDestroyed()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->m_scheduleMetrics.liveScheduleWidgetCount = qMax(
            0,
            profiler->m_scheduleMetrics.liveScheduleWidgetCount - 1
            );
    }
}

void StartupProfiler::recordStartupCompleteScheduleWidgetDiagnostic()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        const StartupApplicationMetrics metrics =
            profiler->applicationMetrics();
        const bool hasExpectedScheduleWidgetCount =
            metrics.liveScheduleWidgetCount == 1
            && metrics.scheduleWidgetsCreated == 1;

        profiler->recordEvent(
            QStringLiteral("schedule-widget-startup-diagnostic"),
            QStringLiteral("expected=1; live=%1; created=%2; passed=%3")
                .arg(metrics.liveScheduleWidgetCount)
                .arg(metrics.scheduleWidgetsCreated)
                .arg(hasExpectedScheduleWidgetCount ? QStringLiteral("true")
                                                    : QStringLiteral("false"))
            );
    }
}

void StartupProfiler::recordScheduleRenderStarted(const QString& owner)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->recordEvent(
            QStringLiteral("schedule-render-start"),
            owner
            );
    }
}

void StartupProfiler::recordScheduleRenderCompleted(
    const QString& owner,
    qint64 elapsedMilliseconds,
    int tableItemsCreated,
    int cellWidgetsCreated,
    int cellWidgetsRemoved,
    int cellWidgetsQueuedForDeletion,
    bool fullRender
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        if (fullRender)
        {
            ++profiler->m_scheduleMetrics.scheduleRenderCount;
        }
        profiler->m_scheduleMetrics.scheduleTableItemsCreated +=
            static_cast<quint64>(qMax(0, tableItemsCreated));
        profiler->m_scheduleMetrics.scheduleCellWidgetsCreated +=
            static_cast<quint64>(qMax(0, cellWidgetsCreated));
        profiler->m_scheduleMetrics.scheduleCellWidgetsRemoved +=
            static_cast<quint64>(qMax(0, cellWidgetsRemoved));
        profiler->m_scheduleMetrics.scheduleCellWidgetsQueuedForDeletion +=
            static_cast<quint64>(qMax(0, cellWidgetsQueuedForDeletion));
        profiler->recordEvent(
            QStringLiteral("schedule-render-end"),
            QStringLiteral("%1; elapsedMs=%2; fullRender=%3; tableItemsCreated=%4; cellWidgetsCreated=%5; cellWidgetsRemoved=%6; cellWidgetsQueuedForDeletion=%7")
                .arg(owner)
                .arg(elapsedMilliseconds)
                .arg(fullRender ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(tableItemsCreated)
                .arg(cellWidgetsCreated)
                .arg(cellWidgetsRemoved)
                .arg(cellWidgetsQueuedForDeletion)
            );
    }
}

void StartupProfiler::recordEvent(
    const QString& name,
    const QString& detail
    )
{
    m_events.append(
        {
            name,
            detail,
            m_timer.elapsed()
        }
        );
}

StartupApplicationMetrics StartupProfiler::applicationMetrics() const
{
    StartupApplicationMetrics metrics = m_scheduleMetrics;

    if (QApplication::instance())
    {
        metrics.widgetCount = QApplication::allWidgets().size();
    }

    if (m_applicationMetricsProvider)
    {
        const StartupApplicationMetrics supplied =
            m_applicationMetricsProvider();
        metrics.instantiatedPageCount = supplied.instantiatedPageCount;
        metrics.registeredPageCount = supplied.registeredPageCount;
    }

    return metrics;
}

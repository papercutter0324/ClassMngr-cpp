#include "startup_profiler.h"

#include <QApplication>
#include <QFileInfo>
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

void appendProfilerWorkflowTrace(const QString& message)
{
    const QString outputPath =
        qEnvironmentVariable("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH")
            .trimmed();
    if (outputPath.isEmpty())
    {
        return;
    }

    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        file.write((message + QLatin1Char('\n')).toUtf8());
        file.flush();
    }
}

QJsonObject applicationMetricsJson(const StartupApplicationMetrics& metrics)
{
    return {
        {QStringLiteral("widgetCount"), metrics.widgetCount},
        {QStringLiteral("instantiatedPageCount"), metrics.instantiatedPageCount},
        {QStringLiteral("registeredPageCount"), metrics.registeredPageCount},
        {
            QStringLiteral("subPrepClassInformationWidgetCount"),
            metrics.subPrepClassInformationWidgetCount
        },
        {
            QStringLiteral("subPrepClassInformationTextEditCount"),
            metrics.subPrepClassInformationTextEditCount
        },
        {
            QStringLiteral("subPrepClassInformationNavigationRowCount"),
            metrics.subPrepClassInformationNavigationRowCount
        },
        {
            QStringLiteral("subPrepClassInformationSourceClassCount"),
            metrics.subPrepClassInformationSourceClassCount
        },
        {
            QStringLiteral("subPrepClassInformationVisibleClassCount"),
            metrics.subPrepClassInformationVisibleClassCount
        },
        {
            QStringLiteral("subPrepClassInformationGroupCount"),
            metrics.subPrepClassInformationGroupCount
        },
        {
            QStringLiteral("subPrepClassInformationClassInfoLookupCount"),
            metrics.subPrepClassInformationClassInfoLookupCount
        },
        {
            QStringLiteral("subPrepClassInformationTeacherLookupCount"),
            metrics.subPrepClassInformationTeacherLookupCount
        },
        {
            QStringLiteral("subPrepClassInformationRosterLookupCount"),
            metrics.subPrepClassInformationRosterLookupCount
        },
        {
            QStringLiteral("subPrepClassInformationClassQueryCount"),
            metrics.subPrepClassInformationClassQueryCount
        },
        {
            QStringLiteral("subPrepClassInformationClassResultRowCount"),
            metrics.subPrepClassInformationClassResultRowCount
        },
        {
            QStringLiteral("subPrepClassInformationClassInfoQueryCount"),
            metrics.subPrepClassInformationClassInfoQueryCount
        },
        {
            QStringLiteral("subPrepClassInformationClassInfoResultRowCount"),
            metrics.subPrepClassInformationClassInfoResultRowCount
        },
        {
            QStringLiteral("subPrepClassInformationClassInfoScheduleRowCount"),
            metrics.subPrepClassInformationClassInfoScheduleRowCount
        },
        {
            QStringLiteral("subPrepClassInformationTeacherQueryCount"),
            metrics.subPrepClassInformationTeacherQueryCount
        },
        {
            QStringLiteral("subPrepClassInformationTeacherResultRowCount"),
            metrics.subPrepClassInformationTeacherResultRowCount
        },
        {
            QStringLiteral("subPrepClassInformationRosterQueryCount"),
            metrics.subPrepClassInformationRosterQueryCount
        },
        {
            QStringLiteral("subPrepClassInformationRosterResultRowCount"),
            metrics.subPrepClassInformationRosterResultRowCount
        },
        {
            QStringLiteral("subPrepClassInformationRosterStudentResultCount"),
            metrics.subPrepClassInformationRosterStudentResultCount
        },
        {
            QStringLiteral("subPrepClassInformationRebuildCount"),
            metrics.subPrepClassInformationRebuildCount
        },
        {
            QStringLiteral("subPrepSelectedClassId"),
            metrics.subPrepSelectedClassId
        },
        {
            QStringLiteral("classesSourceClassCount"),
            metrics.classesSourceClassCount
        },
        {
            QStringLiteral("classesVisibleClassCount"),
            metrics.classesVisibleClassCount
        },
        {
            QStringLiteral("classesNavigationGradeGroupCount"),
            metrics.classesNavigationGradeGroupCount
        },
        {
            QStringLiteral("classesNavigationClassTabCount"),
            metrics.classesNavigationClassTabCount
        },
        {
            QStringLiteral("classesNavigationWidgetCount"),
            metrics.classesNavigationWidgetCount
        },
        {
            QStringLiteral("classesClassQueryCount"),
            metrics.classesClassQueryCount
        },
        {
            QStringLiteral("classesClassResultRowCount"),
            metrics.classesClassResultRowCount
        },
        {
            QStringLiteral("classesClassInfoQueryCount"),
            metrics.classesClassInfoQueryCount
        },
        {
            QStringLiteral("classesClassInfoResultRowCount"),
            metrics.classesClassInfoResultRowCount
        },
        {
            QStringLiteral("classesClassInfoScheduleRowCount"),
            metrics.classesClassInfoScheduleRowCount
        },
        {
            QStringLiteral("classesTeacherQueryCount"),
            metrics.classesTeacherQueryCount
        },
        {
            QStringLiteral("classesTeacherResultRowCount"),
            metrics.classesTeacherResultRowCount
        },
        {
            QStringLiteral("classesVisibleSectionCount"),
            metrics.classesVisibleSectionCount
        },
        {
            QStringLiteral("classesInstantiatedEditorCount"),
            metrics.classesInstantiatedEditorCount
        },
        {
            QStringLiteral("classesLoadedEditorClassCount"),
            metrics.classesLoadedEditorClassCount
        },
        {
            QStringLiteral("classesRebuildCount"),
            metrics.classesRebuildCount
        },
        {
            QStringLiteral("classesSelectedClassId"),
            metrics.classesSelectedClassId
        },
        {QStringLiteral("liveScheduleWidgetCount"), metrics.liveScheduleWidgetCount},
        {QStringLiteral("livePdfDocumentCount"), metrics.livePdfDocumentCount},
        {QStringLiteral("scheduleWidgetsCreated"), static_cast<double>(metrics.scheduleWidgetsCreated)},
        {QStringLiteral("scheduleRenderCount"), static_cast<double>(metrics.scheduleRenderCount)},
        {QStringLiteral("scheduleTableItemsCreated"), static_cast<double>(metrics.scheduleTableItemsCreated)},
        {QStringLiteral("scheduleCellWidgetsCreated"), static_cast<double>(metrics.scheduleCellWidgetsCreated)},
        {QStringLiteral("scheduleCellWidgetsRemoved"), static_cast<double>(metrics.scheduleCellWidgetsRemoved)},
        {
            QStringLiteral("scheduleCellWidgetsQueuedForDeletion"),
            static_cast<double>(metrics.scheduleCellWidgetsQueuedForDeletion)
        },
        {QStringLiteral("pdfDocumentsLoaded"), static_cast<double>(metrics.pdfDocumentsLoaded)},
        {QStringLiteral("pdfDocumentsReleased"), static_cast<double>(metrics.pdfDocumentsReleased)},
        {QStringLiteral("pdfRenderCount"), static_cast<double>(metrics.pdfRenderCount)}
    };
}

QJsonObject peakMemoryJson(
    const QList<StartupCheckpoint>& checkpoints
    )
{
    ProcessMemorySnapshot peak;

    for (const StartupCheckpoint& checkpoint : checkpoints)
    {
        const ProcessMemorySnapshot& memory = checkpoint.memory;

        peak.isAvailable = peak.isAvailable || memory.isAvailable;
        if (peak.platform.isEmpty())
        {
            peak.platform = memory.platform;
        }
        peak.workingSetBytes = qMax(
            peak.workingSetBytes,
            memory.workingSetBytes
            );
        peak.peakWorkingSetBytes = qMax(
            peak.peakWorkingSetBytes,
            memory.peakWorkingSetBytes
            );
        peak.privateUsageBytes = qMax(
            peak.privateUsageBytes,
            memory.privateUsageBytes
            );
        peak.privateWorkingSetBytes = qMax(
            peak.privateWorkingSetBytes,
            memory.privateWorkingSetBytes
            );
        peak.privateDirtyBytes = qMax(
            peak.privateDirtyBytes,
            memory.privateDirtyBytes
            );
        peak.pagefileUsageBytes = qMax(
            peak.pagefileUsageBytes,
            memory.pagefileUsageBytes
            );
        peak.handleCount = qMax(peak.handleCount, memory.handleCount);
        peak.threadCount = qMax(peak.threadCount, memory.threadCount);
    }

    QJsonObject result = memoryJson(peak);
    result.insert(
        QStringLiteral("checkpointSampleCount"),
        checkpoints.size()
        );
    return result;
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
        {QStringLiteral("events"), events},
        {QStringLiteral("peakMemory"), peakMemoryJson(m_checkpoints)}
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

void StartupProfiler::recordPageEntered(const QString& pageIdentifier)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->recordEvent(
            QStringLiteral("page-enter"),
            pageIdentifier
            );
    }
}

void StartupProfiler::recordPageLeft(const QString& pageIdentifier)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->recordEvent(
            QStringLiteral("page-leave"),
            pageIdentifier
            );
    }
}

void StartupProfiler::recordSubPrepClassInformationLifecycle(
    const QString& phase,
    const QString& detail
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        const QString eventDetail =
            detail.isEmpty()
                ? phase
                : QStringLiteral("%1; %2").arg(phase, detail);
        profiler->recordEvent(
            QStringLiteral("sub-prep-class-information"),
            eventDetail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("sub-prep-class-information %1")
                .arg(eventDetail)
            );
    }
}

void StartupProfiler::recordSubPrepRosterQuery(
    int classId,
    int columnCount,
    int rowCount,
    int cellCount,
    int returnedStudentCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        if (!profiler->m_subPrepDiagnosticsActive)
        {
            return;
        }

        const QString detail =
            QStringLiteral(
                "classId=%1; columns=%2; rows=%3; cells=%4; returnedStudents=%5"
                )
                .arg(classId)
                .arg(columnCount)
                .arg(rowCount)
                .arg(cellCount)
                .arg(returnedStudentCount);
        profiler->recordEvent(
            QStringLiteral("sub-prep-roster-query"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("sub-prep-roster-query %1")
                .arg(detail)
            );
    }
}

void StartupProfiler::setSubPrepDiagnosticsActive(bool active)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->m_subPrepDiagnosticsActive = active;
    }
}

void StartupProfiler::recordPdfDocumentLoaded(
    const QString& filePath,
    int pageCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.livePdfDocumentCount;
        ++profiler->m_scheduleMetrics.pdfDocumentsLoaded;
        profiler->recordEvent(
            QStringLiteral("pdf-document-loaded"),
            QStringLiteral("%1; pages=%2")
                .arg(
                    QFileInfo(filePath).fileName(),
                    QString::number(pageCount)
                    )
            );
    }
}

void StartupProfiler::recordPdfDocumentReleased(const QString& filePath)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->m_scheduleMetrics.livePdfDocumentCount = qMax(
            0,
            profiler->m_scheduleMetrics.livePdfDocumentCount - 1
            );
        ++profiler->m_scheduleMetrics.pdfDocumentsReleased;
        profiler->recordEvent(
            QStringLiteral("pdf-document-released"),
            QFileInfo(filePath).fileName()
            );
    }
}

void StartupProfiler::recordPdfDocumentRendered(
    const QString& filePath,
    int width,
    int height
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.pdfRenderCount;
        profiler->recordEvent(
            QStringLiteral("pdf-document-rendered"),
            QStringLiteral("%1; size=%2x%3")
                .arg(
                    QFileInfo(filePath).fileName(),
                    QString::number(width),
                    QString::number(height)
                    )
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
        metrics.subPrepClassInformationWidgetCount =
            supplied.subPrepClassInformationWidgetCount;
        metrics.subPrepClassInformationTextEditCount =
            supplied.subPrepClassInformationTextEditCount;
        metrics.subPrepClassInformationNavigationRowCount =
            supplied.subPrepClassInformationNavigationRowCount;
        metrics.subPrepClassInformationSourceClassCount =
            supplied.subPrepClassInformationSourceClassCount;
        metrics.subPrepClassInformationVisibleClassCount =
            supplied.subPrepClassInformationVisibleClassCount;
        metrics.subPrepClassInformationGroupCount =
            supplied.subPrepClassInformationGroupCount;
        metrics.subPrepClassInformationClassInfoLookupCount =
            supplied.subPrepClassInformationClassInfoLookupCount;
        metrics.subPrepClassInformationTeacherLookupCount =
            supplied.subPrepClassInformationTeacherLookupCount;
        metrics.subPrepClassInformationRosterLookupCount =
            supplied.subPrepClassInformationRosterLookupCount;
        metrics.subPrepClassInformationClassQueryCount =
            supplied.subPrepClassInformationClassQueryCount;
        metrics.subPrepClassInformationClassResultRowCount =
            supplied.subPrepClassInformationClassResultRowCount;
        metrics.subPrepClassInformationClassInfoQueryCount =
            supplied.subPrepClassInformationClassInfoQueryCount;
        metrics.subPrepClassInformationClassInfoResultRowCount =
            supplied.subPrepClassInformationClassInfoResultRowCount;
        metrics.subPrepClassInformationClassInfoScheduleRowCount =
            supplied.subPrepClassInformationClassInfoScheduleRowCount;
        metrics.subPrepClassInformationTeacherQueryCount =
            supplied.subPrepClassInformationTeacherQueryCount;
        metrics.subPrepClassInformationTeacherResultRowCount =
            supplied.subPrepClassInformationTeacherResultRowCount;
        metrics.subPrepClassInformationRosterQueryCount =
            supplied.subPrepClassInformationRosterQueryCount;
        metrics.subPrepClassInformationRosterResultRowCount =
            supplied.subPrepClassInformationRosterResultRowCount;
        metrics.subPrepClassInformationRosterStudentResultCount =
            supplied.subPrepClassInformationRosterStudentResultCount;
        metrics.subPrepClassInformationRebuildCount =
            supplied.subPrepClassInformationRebuildCount;
        metrics.subPrepSelectedClassId =
            supplied.subPrepSelectedClassId;
        metrics.classesSourceClassCount =
            supplied.classesSourceClassCount;
        metrics.classesVisibleClassCount =
            supplied.classesVisibleClassCount;
        metrics.classesNavigationGradeGroupCount =
            supplied.classesNavigationGradeGroupCount;
        metrics.classesNavigationClassTabCount =
            supplied.classesNavigationClassTabCount;
        metrics.classesNavigationWidgetCount =
            supplied.classesNavigationWidgetCount;
        metrics.classesClassQueryCount =
            supplied.classesClassQueryCount;
        metrics.classesClassResultRowCount =
            supplied.classesClassResultRowCount;
        metrics.classesClassInfoQueryCount =
            supplied.classesClassInfoQueryCount;
        metrics.classesClassInfoResultRowCount =
            supplied.classesClassInfoResultRowCount;
        metrics.classesClassInfoScheduleRowCount =
            supplied.classesClassInfoScheduleRowCount;
        metrics.classesTeacherQueryCount =
            supplied.classesTeacherQueryCount;
        metrics.classesTeacherResultRowCount =
            supplied.classesTeacherResultRowCount;
        metrics.classesVisibleSectionCount =
            supplied.classesVisibleSectionCount;
        metrics.classesInstantiatedEditorCount =
            supplied.classesInstantiatedEditorCount;
        metrics.classesLoadedEditorClassCount =
            supplied.classesLoadedEditorClassCount;
        metrics.classesRebuildCount =
            supplied.classesRebuildCount;
        metrics.classesSelectedClassId =
            supplied.classesSelectedClassId;
    }

    return metrics;
}

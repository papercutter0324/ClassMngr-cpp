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
        {
            QStringLiteral("scheduleModelRowCount"),
            metrics.scheduleModelRowCount
        },
        {
            QStringLiteral("scheduleModelCellCount"),
            metrics.scheduleModelCellCount
        },
        {
            QStringLiteral("scheduleModelEntryCount"),
            metrics.scheduleModelEntryCount
        },
        {
            QStringLiteral("scheduleTableRowCount"),
            metrics.scheduleTableRowCount
        },
        {
            QStringLiteral("scheduleTableColumnCount"),
            metrics.scheduleTableColumnCount
        },
        {
            QStringLiteral("scheduleTableItemCount"),
            metrics.scheduleTableItemCount
        },
        {
            QStringLiteral("scheduleTableCellWidgetCount"),
            metrics.scheduleTableCellWidgetCount
        },
        {
            QStringLiteral("scheduleVisibleClassCount"),
            metrics.scheduleVisibleClassCount
        },
        {
            QStringLiteral("calendarCacheEventCount"),
            metrics.calendarCacheEventCount
        },
        {
            QStringLiteral("calendarCacheDateBucketCount"),
            metrics.calendarCacheDateBucketCount
        },
        {
            QStringLiteral("calendarCacheLoadedRangeCount"),
            metrics.calendarCacheLoadedRangeCount
        },
        {
            QStringLiteral("calendarCacheRetainedRangeCount"),
            metrics.calendarCacheRetainedRangeCount
        },
        {
            QStringLiteral("calendarLoadedMonthCount"),
            metrics.calendarLoadedMonthCount
        },
        {
            QStringLiteral("calendarOnDemandRetainedRangeCount"),
            metrics.calendarOnDemandRetainedRangeCount
        },
        {
            QStringLiteral("calendarModelRevision"),
            metrics.calendarModelRevision
        },
        {
            QStringLiteral("calendarPageWidgetCount"),
            metrics.calendarPageWidgetCount
        },
        {
            QStringLiteral("calendarViewObjectCount"),
            metrics.calendarViewObjectCount
        },
        {
            QStringLiteral("calendarCacheLoading"),
            metrics.calendarCacheLoading
        },
        {
            QStringLiteral("scheduleImportWorkbookSheetCount"),
            metrics.scheduleImportWorkbookSheetCount
        },
        {
            QStringLiteral("scheduleImportWorkbookUserCount"),
            metrics.scheduleImportWorkbookUserCount
        },
        {
            QStringLiteral("scheduleImportWorkbookClassCandidateCount"),
            metrics.scheduleImportWorkbookClassCandidateCount
        },
        {
            QStringLiteral("scheduleImportWorkbookDiagnosticCount"),
            metrics.scheduleImportWorkbookDiagnosticCount
        },
        {
            QStringLiteral("scheduleImportPreviewTeacherCount"),
            metrics.scheduleImportPreviewTeacherCount
        },
        {
            QStringLiteral("scheduleImportPreviewClassCount"),
            metrics.scheduleImportPreviewClassCount
        },
        {
            QStringLiteral("scheduleImportPreviewDiagnosticCount"),
            metrics.scheduleImportPreviewDiagnosticCount
        },
        {
            QStringLiteral("scheduleImportReviewTeacherControlCount"),
            metrics.scheduleImportReviewTeacherControlCount
        },
        {
            QStringLiteral("scheduleImportReviewClassControlCount"),
            metrics.scheduleImportReviewClassControlCount
        },
        {
            QStringLiteral("scheduleImportReviewPreviewEntryCount"),
            metrics.scheduleImportReviewPreviewEntryCount
        },
        {
            QStringLiteral("scheduleImportRawWorkbookBytes"),
            static_cast<double>(metrics.scheduleImportRawWorkbookBytes)
        },
        {
            QStringLiteral("calendarImportWorkbookSheetCount"),
            metrics.calendarImportWorkbookSheetCount
        },
        {
            QStringLiteral("calendarImportWorkbookCellCount"),
            metrics.calendarImportWorkbookCellCount
        },
        {
            QStringLiteral("calendarImportWorkbookMergedRangeCount"),
            metrics.calendarImportWorkbookMergedRangeCount
        },
        {
            QStringLiteral("calendarImportWorkbookSharedStringCount"),
            metrics.calendarImportWorkbookSharedStringCount
        },
        {
            QStringLiteral("calendarImportWorkbookStyleCount"),
            metrics.calendarImportWorkbookStyleCount
        },
        {
            QStringLiteral("calendarImportParsedEventCount"),
            metrics.calendarImportParsedEventCount
        },
        {
            QStringLiteral("calendarImportParsedSkippedCount"),
            metrics.calendarImportParsedSkippedCount
        },
        {
            QStringLiteral("calendarImportExistingEventCount"),
            metrics.calendarImportExistingEventCount
        },
        {
            QStringLiteral("calendarImportEventsToSaveCount"),
            metrics.calendarImportEventsToSaveCount
        },
        {
            QStringLiteral("calendarImportSavedEventCount"),
            metrics.calendarImportSavedEventCount
        },
        {
            QStringLiteral("scheduleImportRawBytesRetained"),
            metrics.scheduleImportRawBytesRetained
        },
        {
            QStringLiteral("scheduleImportWorkbookRetained"),
            metrics.scheduleImportWorkbookRetained
        },
        {
            QStringLiteral("scheduleImportReviewRetained"),
            metrics.scheduleImportReviewRetained
        },
        {
            QStringLiteral("calendarImportRawWorkbookBytes"),
            static_cast<double>(metrics.calendarImportRawWorkbookBytes)
        },
        {
            QStringLiteral("calendarImportRawBytesRetained"),
            metrics.calendarImportRawBytesRetained
        },
        {
            QStringLiteral("calendarImportWorkbookRetained"),
            metrics.calendarImportWorkbookRetained
        },
        {
            QStringLiteral("calendarImportEventsRetained"),
            metrics.calendarImportEventsRetained
        },
        {
            QStringLiteral("calendarImportOperationRetained"),
            metrics.calendarImportOperationRetained
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
        {
            QStringLiteral("scheduleImportOperationsStarted"),
            static_cast<double>(metrics.scheduleImportOperationsStarted)
        },
        {
            QStringLiteral("scheduleImportWorkbooksLoaded"),
            static_cast<double>(metrics.scheduleImportWorkbooksLoaded)
        },
        {
            QStringLiteral("scheduleImportReviewsPrepared"),
            static_cast<double>(metrics.scheduleImportReviewsPrepared)
        },
        {
            QStringLiteral("scheduleImportReviewsReleased"),
            static_cast<double>(metrics.scheduleImportReviewsReleased)
        },
        {
            QStringLiteral("scheduleImportOperationsCancelled"),
            static_cast<double>(metrics.scheduleImportOperationsCancelled)
        },
        {
            QStringLiteral("scheduleImportOperationsApplied"),
            static_cast<double>(metrics.scheduleImportOperationsApplied)
        },
        {
            QStringLiteral("scheduleImportOperationsReleased"),
            static_cast<double>(metrics.scheduleImportOperationsReleased)
        },
        {
            QStringLiteral("calendarImportOperationsStarted"),
            static_cast<double>(metrics.calendarImportOperationsStarted)
        },
        {
            QStringLiteral("calendarImportResponsesReceived"),
            static_cast<double>(metrics.calendarImportResponsesReceived)
        },
        {
            QStringLiteral("calendarImportWorkbooksParsed"),
            static_cast<double>(metrics.calendarImportWorkbooksParsed)
        },
        {
            QStringLiteral("calendarImportOperationsApplied"),
            static_cast<double>(metrics.calendarImportOperationsApplied)
        },
        {
            QStringLiteral("calendarImportOperationsFailed"),
            static_cast<double>(metrics.calendarImportOperationsFailed)
        },
        {
            QStringLiteral("calendarImportOperationsReleased"),
            static_cast<double>(metrics.calendarImportOperationsReleased)
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

void StartupProfiler::recordScheduleImportStarted(
    const QString& filePath,
    qint64 rawWorkbookBytes
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.scheduleImportOperationsStarted;
        metrics.scheduleImportRawWorkbookBytes =
            qMax<qint64>(0, rawWorkbookBytes);
        metrics.scheduleImportRawBytesRetained = rawWorkbookBytes > 0;
        metrics.scheduleImportWorkbookRetained = false;
        metrics.scheduleImportReviewRetained = false;
        metrics.scheduleImportWorkbookSheetCount = 0;
        metrics.scheduleImportWorkbookUserCount = 0;
        metrics.scheduleImportWorkbookClassCandidateCount = 0;
        metrics.scheduleImportWorkbookDiagnosticCount = 0;
        metrics.scheduleImportPreviewTeacherCount = 0;
        metrics.scheduleImportPreviewClassCount = 0;
        metrics.scheduleImportPreviewDiagnosticCount = 0;
        metrics.scheduleImportReviewTeacherControlCount = 0;
        metrics.scheduleImportReviewClassControlCount = 0;
        metrics.scheduleImportReviewPreviewEntryCount = 0;

        const QString detail =
            QStringLiteral("path=%1; rawBytes=%2")
                .arg(filePath)
                .arg(qMax<qint64>(0, rawWorkbookBytes));
        profiler->recordEvent(
            QStringLiteral("schedule-import-operation-start"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-operation-start %1")
                .arg(detail)
            );
    }
}

void StartupProfiler::recordScheduleImportWorkbookLoaded(
    int sheetCount,
    int userCount,
    int classCandidateCount,
    int diagnosticCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.scheduleImportWorkbooksLoaded;
        metrics.scheduleImportRawBytesRetained = false;
        metrics.scheduleImportWorkbookRetained = true;
        metrics.scheduleImportWorkbookSheetCount = qMax(0, sheetCount);
        metrics.scheduleImportWorkbookUserCount = qMax(0, userCount);
        metrics.scheduleImportWorkbookClassCandidateCount =
            qMax(0, classCandidateCount);
        metrics.scheduleImportWorkbookDiagnosticCount =
            qMax(0, diagnosticCount);

        const QString detail =
            QStringLiteral(
                "sheets=%1; users=%2; classCandidates=%3; diagnostics=%4; rawBytesRetained=false"
                )
                .arg(metrics.scheduleImportWorkbookSheetCount)
                .arg(metrics.scheduleImportWorkbookUserCount)
                .arg(metrics.scheduleImportWorkbookClassCandidateCount)
                .arg(metrics.scheduleImportWorkbookDiagnosticCount);
        profiler->recordEvent(
            QStringLiteral("schedule-import-workbook-loaded"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-workbook-loaded %1")
                .arg(detail)
            );
    }
}

void StartupProfiler::recordScheduleImportReviewPrepared(
    int teacherCount,
    int classCount,
    int diagnosticCount,
    int previewEntryCount,
    int teacherControlCount,
    int classControlCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.scheduleImportReviewsPrepared;
        metrics.scheduleImportReviewRetained = true;
        metrics.scheduleImportPreviewTeacherCount = qMax(0, teacherCount);
        metrics.scheduleImportPreviewClassCount = qMax(0, classCount);
        metrics.scheduleImportPreviewDiagnosticCount = qMax(0, diagnosticCount);
        metrics.scheduleImportReviewPreviewEntryCount =
            qMax(0, previewEntryCount);
        metrics.scheduleImportReviewTeacherControlCount =
            qMax(0, teacherControlCount);
        metrics.scheduleImportReviewClassControlCount =
            qMax(0, classControlCount);

        const QString detail =
            QStringLiteral(
                "teachers=%1; classes=%2; diagnostics=%3; previewEntries=%4; teacherControls=%5; classControls=%6"
                )
                .arg(metrics.scheduleImportPreviewTeacherCount)
                .arg(metrics.scheduleImportPreviewClassCount)
                .arg(metrics.scheduleImportPreviewDiagnosticCount)
                .arg(metrics.scheduleImportReviewPreviewEntryCount)
                .arg(metrics.scheduleImportReviewTeacherControlCount)
                .arg(metrics.scheduleImportReviewClassControlCount);
        profiler->recordEvent(
            QStringLiteral("schedule-import-review-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-review-prepared %1")
                .arg(detail)
            );
    }
}

void StartupProfiler::recordScheduleImportReviewReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.scheduleImportReviewsReleased;
        metrics.scheduleImportReviewRetained = false;
        metrics.scheduleImportPreviewTeacherCount = 0;
        metrics.scheduleImportPreviewClassCount = 0;
        metrics.scheduleImportPreviewDiagnosticCount = 0;
        metrics.scheduleImportReviewTeacherControlCount = 0;
        metrics.scheduleImportReviewClassControlCount = 0;
        metrics.scheduleImportReviewPreviewEntryCount = 0;
        profiler->recordEvent(
            QStringLiteral("schedule-import-review-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-review-released")
            );
    }
}

void StartupProfiler::recordScheduleImportCancelled()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.scheduleImportOperationsCancelled;
        profiler->recordEvent(
            QStringLiteral("schedule-import-operation-cancelled")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-operation-cancelled")
            );
    }
}

void StartupProfiler::recordScheduleImportApplied()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.scheduleImportOperationsApplied;
        profiler->recordEvent(
            QStringLiteral("schedule-import-operation-applied")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-operation-applied")
            );
    }
}

void StartupProfiler::recordScheduleImportOperationReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.scheduleImportOperationsReleased;
        metrics.scheduleImportRawBytesRetained = false;
        metrics.scheduleImportWorkbookRetained = false;
        metrics.scheduleImportReviewRetained = false;
        metrics.scheduleImportWorkbookSheetCount = 0;
        metrics.scheduleImportWorkbookUserCount = 0;
        metrics.scheduleImportWorkbookClassCandidateCount = 0;
        metrics.scheduleImportWorkbookDiagnosticCount = 0;
        profiler->recordEvent(
            QStringLiteral("schedule-import-operation-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-operation-released")
            );
    }
}

void StartupProfiler::recordCalendarImportStarted(
    const QString& sourceUrl
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.calendarImportOperationsStarted;
        metrics.calendarImportOperationRetained = true;
        metrics.calendarImportRawBytesRetained = false;
        metrics.calendarImportWorkbookRetained = false;
        metrics.calendarImportEventsRetained = false;
        metrics.calendarImportRawWorkbookBytes = 0;
        metrics.calendarImportWorkbookSheetCount = 0;
        metrics.calendarImportWorkbookCellCount = 0;
        metrics.calendarImportWorkbookMergedRangeCount = 0;
        metrics.calendarImportWorkbookSharedStringCount = 0;
        metrics.calendarImportWorkbookStyleCount = 0;
        metrics.calendarImportParsedEventCount = 0;
        metrics.calendarImportParsedSkippedCount = 0;
        metrics.calendarImportExistingEventCount = 0;
        metrics.calendarImportEventsToSaveCount = 0;
        metrics.calendarImportSavedEventCount = 0;

        const QString detail =
            QStringLiteral("source=%1")
                .arg(sourceUrl);
        profiler->recordEvent(
            QStringLiteral("calendar-import-operation-start"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-operation-start %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-operation-start"),
            detail
            );
    }
}

void StartupProfiler::recordCalendarImportResponseReceived(
    qint64 bytes
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.calendarImportResponsesReceived;
        metrics.calendarImportRawWorkbookBytes = qMax<qint64>(0, bytes);
        metrics.calendarImportRawBytesRetained = true;

        const QString detail =
            QStringLiteral("rawBytes=%1; rawBytesRetained=true")
                .arg(metrics.calendarImportRawWorkbookBytes);
        profiler->recordEvent(
            QStringLiteral("calendar-import-response-received"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-response-received %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-response-received"),
            detail
            );
    }
}

void StartupProfiler::recordCalendarImportWorkbookParsed(
    int sheetCount,
    int cellCount,
    int mergedRangeCount,
    int sharedStringCount,
    int styleCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.calendarImportWorkbooksParsed;
        metrics.calendarImportRawBytesRetained = false;
        metrics.calendarImportWorkbookRetained = true;
        metrics.calendarImportWorkbookSheetCount = qMax(0, sheetCount);
        metrics.calendarImportWorkbookCellCount = qMax(0, cellCount);
        metrics.calendarImportWorkbookMergedRangeCount = qMax(0, mergedRangeCount);
        metrics.calendarImportWorkbookSharedStringCount = qMax(0, sharedStringCount);
        metrics.calendarImportWorkbookStyleCount = qMax(0, styleCount);

        const QString detail =
            QStringLiteral(
                "sheets=%1; cells=%2; mergedRanges=%3; sharedStrings=%4; styles=%5; rawBytesRetained=false; workbookRetained=true"
                )
                .arg(metrics.calendarImportWorkbookSheetCount)
                .arg(metrics.calendarImportWorkbookCellCount)
                .arg(metrics.calendarImportWorkbookMergedRangeCount)
                .arg(metrics.calendarImportWorkbookSharedStringCount)
                .arg(metrics.calendarImportWorkbookStyleCount);
        profiler->recordEvent(
            QStringLiteral("calendar-import-workbook-parsed"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-workbook-parsed %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-workbook-parsed"),
            detail
            );
    }
}

void StartupProfiler::recordCalendarImportEventsPrepared(
    int eventCount,
    int skippedCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.calendarImportEventsRetained = true;
        metrics.calendarImportParsedEventCount = qMax(0, eventCount);
        metrics.calendarImportParsedSkippedCount = qMax(0, skippedCount);

        const QString detail =
            QStringLiteral("events=%1; skipped=%2; eventsRetained=true")
                .arg(metrics.calendarImportParsedEventCount)
                .arg(metrics.calendarImportParsedSkippedCount);
        profiler->recordEvent(
            QStringLiteral("calendar-import-events-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-events-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-events-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordCalendarImportExistingEventsLoaded(
    int eventCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.calendarImportExistingEventCount = qMax(0, eventCount);

        const QString detail =
            QStringLiteral("existingEvents=%1")
                .arg(metrics.calendarImportExistingEventCount);
        profiler->recordEvent(
            QStringLiteral("calendar-import-existing-events-loaded"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-existing-events-loaded %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-existing-events-loaded"),
            detail
            );
    }
}

void StartupProfiler::recordCalendarImportSavePrepared(
    int eventsToSaveCount,
    int skippedCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.calendarImportEventsToSaveCount = qMax(0, eventsToSaveCount);
        metrics.calendarImportParsedSkippedCount = qMax(0, skippedCount);

        const QString detail =
            QStringLiteral("eventsToSave=%1; skipped=%2")
                .arg(metrics.calendarImportEventsToSaveCount)
                .arg(metrics.calendarImportParsedSkippedCount);
        profiler->recordEvent(
            QStringLiteral("calendar-import-save-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-save-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-save-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordCalendarImportApplied(
    int savedCount,
    int skippedCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.calendarImportOperationsApplied;
        metrics.calendarImportSavedEventCount = qMax(0, savedCount);
        metrics.calendarImportParsedSkippedCount = qMax(0, skippedCount);

        const QString detail =
            QStringLiteral("saved=%1; skipped=%2")
                .arg(metrics.calendarImportSavedEventCount)
                .arg(metrics.calendarImportParsedSkippedCount);
        profiler->recordEvent(
            QStringLiteral("calendar-import-operation-applied"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-operation-applied %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-operation-applied"),
            detail
            );
    }
}

void StartupProfiler::recordCalendarImportFailed(
    const QString& detail
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.calendarImportOperationsFailed;
        profiler->recordEvent(
            QStringLiteral("calendar-import-operation-failed"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-operation-failed %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-operation-failed"),
            detail
            );
    }
}

void StartupProfiler::recordCalendarImportOperationReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.calendarImportOperationsReleased;
        metrics.calendarImportRawBytesRetained = false;
        metrics.calendarImportWorkbookRetained = false;
        metrics.calendarImportEventsRetained = false;
        metrics.calendarImportOperationRetained = false;

        profiler->recordEvent(
            QStringLiteral("calendar-import-operation-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("calendar-import-operation-released")
            );
        profiler->checkpoint(
            QStringLiteral("calendar-import-operation-released")
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
        metrics.scheduleModelRowCount =
            supplied.scheduleModelRowCount;
        metrics.scheduleModelCellCount =
            supplied.scheduleModelCellCount;
        metrics.scheduleModelEntryCount =
            supplied.scheduleModelEntryCount;
        metrics.scheduleTableRowCount =
            supplied.scheduleTableRowCount;
        metrics.scheduleTableColumnCount =
            supplied.scheduleTableColumnCount;
        metrics.scheduleTableItemCount =
            supplied.scheduleTableItemCount;
        metrics.scheduleTableCellWidgetCount =
            supplied.scheduleTableCellWidgetCount;
        metrics.scheduleVisibleClassCount =
            supplied.scheduleVisibleClassCount;
        metrics.calendarCacheEventCount =
            supplied.calendarCacheEventCount;
        metrics.calendarCacheDateBucketCount =
            supplied.calendarCacheDateBucketCount;
        metrics.calendarCacheLoadedRangeCount =
            supplied.calendarCacheLoadedRangeCount;
        metrics.calendarCacheRetainedRangeCount =
            supplied.calendarCacheRetainedRangeCount;
        metrics.calendarLoadedMonthCount =
            supplied.calendarLoadedMonthCount;
        metrics.calendarOnDemandRetainedRangeCount =
            supplied.calendarOnDemandRetainedRangeCount;
        metrics.calendarModelRevision =
            supplied.calendarModelRevision;
        metrics.calendarPageWidgetCount =
            supplied.calendarPageWidgetCount;
        metrics.calendarViewObjectCount =
            supplied.calendarViewObjectCount;
        metrics.calendarCacheLoading =
            supplied.calendarCacheLoading;
    }

    return metrics;
}

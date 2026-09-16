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
            QStringLiteral("scheduleImportExistingTeacherCount"),
            metrics.scheduleImportExistingTeacherCount
        },
        {
            QStringLiteral("scheduleImportExistingClassCount"),
            metrics.scheduleImportExistingClassCount
        },
        {
            QStringLiteral("scheduleImportExistingClassInfoCount"),
            metrics.scheduleImportExistingClassInfoCount
        },
        {
            QStringLiteral("scheduleImportApplyFinalClassCount"),
            metrics.scheduleImportApplyFinalClassCount
        },
        {
            QStringLiteral("scheduleImportApplyFinalScheduleRowCount"),
            metrics.scheduleImportApplyFinalScheduleRowCount
        },
        {
            QStringLiteral("scheduleImportApplyTeacherResolutionCount"),
            metrics.scheduleImportApplyTeacherResolutionCount
        },
        {
            QStringLiteral("scheduleImportApplyClassResolutionCount"),
            metrics.scheduleImportApplyClassResolutionCount
        },
        {
            QStringLiteral("scheduleImportTeachersCreated"),
            metrics.scheduleImportTeachersCreated
        },
        {
            QStringLiteral("scheduleImportTeachersUpdated"),
            metrics.scheduleImportTeachersUpdated
        },
        {
            QStringLiteral("scheduleImportClassesCreated"),
            metrics.scheduleImportClassesCreated
        },
        {
            QStringLiteral("scheduleImportClassesUpdated"),
            metrics.scheduleImportClassesUpdated
        },
        {
            QStringLiteral("scheduleImportClassesSkipped"),
            metrics.scheduleImportClassesSkipped
        },
        {
            QStringLiteral("scheduleImportSchedulesCleared"),
            metrics.scheduleImportSchedulesCleared
        },
        {
            QStringLiteral("scheduleImportIgnoredCells"),
            metrics.scheduleImportIgnoredCells
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
            QStringLiteral("classTransferPackageTeacherCount"),
            metrics.classTransferPackageTeacherCount
        },
        {
            QStringLiteral("classTransferPackageClassCount"),
            metrics.classTransferPackageClassCount
        },
        {
            QStringLiteral("classTransferPackageRosterColumnCount"),
            metrics.classTransferPackageRosterColumnCount
        },
        {
            QStringLiteral("classTransferPackageRosterRowCount"),
            metrics.classTransferPackageRosterRowCount
        },
        {
            QStringLiteral("classTransferPackageRosterCellCount"),
            metrics.classTransferPackageRosterCellCount
        },
        {
            QStringLiteral("classTransferPackageEvaluationCount"),
            metrics.classTransferPackageEvaluationCount
        },
        {
            QStringLiteral("classTransferPackageEvaluationRowCount"),
            metrics.classTransferPackageEvaluationRowCount
        },
        {
            QStringLiteral("classTransferPackageEvaluationCellCount"),
            metrics.classTransferPackageEvaluationCellCount
        },
        {
            QStringLiteral("classTransferPackageScheduleRowCount"),
            metrics.classTransferPackageScheduleRowCount
        },
        {
            QStringLiteral("classTransferPreviewTeacherCount"),
            metrics.classTransferPreviewTeacherCount
        },
        {
            QStringLiteral("classTransferPreviewClassCount"),
            metrics.classTransferPreviewClassCount
        },
        {
            QStringLiteral("classTransferMatchingTeacherCount"),
            metrics.classTransferMatchingTeacherCount
        },
        {
            QStringLiteral("classTransferMatchingClassCount"),
            metrics.classTransferMatchingClassCount
        },
        {
            QStringLiteral("classTransferDestinationTeacherCount"),
            metrics.classTransferDestinationTeacherCount
        },
        {
            QStringLiteral("classTransferDestinationClassCount"),
            metrics.classTransferDestinationClassCount
        },
        {
            QStringLiteral("classTransferDestinationClassInfoResultCount"),
            metrics.classTransferDestinationClassInfoResultCount
        },
        {
            QStringLiteral("classTransferDialogTeacherControlCount"),
            metrics.classTransferDialogTeacherControlCount
        },
        {
            QStringLiteral("classTransferDialogClassControlCount"),
            metrics.classTransferDialogClassControlCount
        },
        {
            QStringLiteral("classTransferPlanTeacherResolutionCount"),
            metrics.classTransferPlanTeacherResolutionCount
        },
        {
            QStringLiteral("classTransferPlanClassResolutionCount"),
            metrics.classTransferPlanClassResolutionCount
        },
        {
            QStringLiteral("classTransferTeachersCreated"),
            metrics.classTransferTeachersCreated
        },
        {
            QStringLiteral("classTransferTeachersKept"),
            metrics.classTransferTeachersKept
        },
        {
            QStringLiteral("classTransferTeachersReplaced"),
            metrics.classTransferTeachersReplaced
        },
        {
            QStringLiteral("classTransferClassesCreated"),
            metrics.classTransferClassesCreated
        },
        {
            QStringLiteral("classTransferClassesReplaced"),
            metrics.classTransferClassesReplaced
        },
        {
            QStringLiteral("classTransferClassesSkipped"),
            metrics.classTransferClassesSkipped
        },
        {
            QStringLiteral("classTransferDestinationClassesBefore"),
            metrics.classTransferDestinationClassesBefore
        },
        {
            QStringLiteral("classTransferDestinationClassesAfter"),
            metrics.classTransferDestinationClassesAfter
        },
        {
            QStringLiteral("classTransferDestinationTeachersBefore"),
            metrics.classTransferDestinationTeachersBefore
        },
        {
            QStringLiteral("classTransferDestinationTeachersAfter"),
            metrics.classTransferDestinationTeachersAfter
        },
        {
            QStringLiteral("speakingEvalSourceClassCount"),
            metrics.speakingEvalSourceClassCount
        },
        {
            QStringLiteral("speakingEvalVisibleClassCount"),
            metrics.speakingEvalVisibleClassCount
        },
        {
            QStringLiteral("speakingEvalClassTabWidgetCount"),
            metrics.speakingEvalClassTabWidgetCount
        },
        {
            QStringLiteral("speakingEvalClassTabCount"),
            metrics.speakingEvalClassTabCount
        },
        {
            QStringLiteral("speakingEvalEvaluationTabCount"),
            metrics.speakingEvalEvaluationTabCount
        },
        {
            QStringLiteral("speakingEvalModelRowCount"),
            metrics.speakingEvalModelRowCount
        },
        {
            QStringLiteral("speakingEvalModelColumnCount"),
            metrics.speakingEvalModelColumnCount
        },
        {
            QStringLiteral("speakingEvalModelCellCount"),
            metrics.speakingEvalModelCellCount
        },
        {
            QStringLiteral("speakingEvalLoadedEvaluationCount"),
            metrics.speakingEvalLoadedEvaluationCount
        },
        {
            QStringLiteral("speakingEvalLoadedEvaluationRowCount"),
            metrics.speakingEvalLoadedEvaluationRowCount
        },
        {
            QStringLiteral("speakingEvalLoadedEvaluationCellCount"),
            metrics.speakingEvalLoadedEvaluationCellCount
        },
        {
            QStringLiteral("speakingEvalBatchReportCount"),
            metrics.speakingEvalBatchReportCount
        },
        {
            QStringLiteral("speakingEvalReportDialogReportCount"),
            metrics.speakingEvalReportDialogReportCount
        },
        {
            QStringLiteral("speakingEvalExportDialogReportCount"),
            metrics.speakingEvalExportDialogReportCount
        },
        {
            QStringLiteral("speakingEvalAiDialogReportCount"),
            metrics.speakingEvalAiDialogReportCount
        },
        {
            QStringLiteral("speakingEvalAiSelectionRowCount"),
            metrics.speakingEvalAiSelectionRowCount
        },
        {
            QStringLiteral("speakingEvalAiSelectionColumnCount"),
            metrics.speakingEvalAiSelectionColumnCount
        },
        {
            QStringLiteral("speakingEvalAiSelectionItemCount"),
            metrics.speakingEvalAiSelectionItemCount
        },
        {
            QStringLiteral("speakingEvalAiReviewRowCount"),
            metrics.speakingEvalAiReviewRowCount
        },
        {
            QStringLiteral("speakingEvalAiReviewColumnCount"),
            metrics.speakingEvalAiReviewColumnCount
        },
        {
            QStringLiteral("speakingEvalAiReviewItemCount"),
            metrics.speakingEvalAiReviewItemCount
        },
        {
            QStringLiteral("speakingEvalAiAcceptedCommentCount"),
            metrics.speakingEvalAiAcceptedCommentCount
        },
        {
            QStringLiteral("speakingEvalExportPdfCount"),
            metrics.speakingEvalExportPdfCount
        },
        {
            QStringLiteral("staffDirectoryNativeRowCount"),
            metrics.staffDirectoryNativeRowCount
        },
        {
            QStringLiteral("staffDirectoryNativeColumnCount"),
            metrics.staffDirectoryNativeColumnCount
        },
        {
            QStringLiteral("staffDirectoryNativeItemCount"),
            metrics.staffDirectoryNativeItemCount
        },
        {
            QStringLiteral("staffDirectoryNativePageWidgetCount"),
            metrics.staffDirectoryNativePageWidgetCount
        },
        {
            QStringLiteral("staffDirectoryNativeRefreshCount"),
            metrics.staffDirectoryNativeRefreshCount
        },
        {
            QStringLiteral("staffDirectoryNativeReentryCount"),
            metrics.staffDirectoryNativeReentryCount
        },
        {
            QStringLiteral("staffDirectoryGsRowCount"),
            metrics.staffDirectoryGsRowCount
        },
        {
            QStringLiteral("staffDirectoryGsColumnCount"),
            metrics.staffDirectoryGsColumnCount
        },
        {
            QStringLiteral("staffDirectoryGsItemCount"),
            metrics.staffDirectoryGsItemCount
        },
        {
            QStringLiteral("staffDirectoryGsPageWidgetCount"),
            metrics.staffDirectoryGsPageWidgetCount
        },
        {
            QStringLiteral("staffDirectoryGsRefreshCount"),
            metrics.staffDirectoryGsRefreshCount
        },
        {
            QStringLiteral("staffDirectoryGsReentryCount"),
            metrics.staffDirectoryGsReentryCount
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
            QStringLiteral("classTransferRawJsonBytes"),
            static_cast<double>(metrics.classTransferRawJsonBytes)
        },
        {
            QStringLiteral("speakingEvalBatchReportTextBytes"),
            static_cast<double>(metrics.speakingEvalBatchReportTextBytes)
        },
        {
            QStringLiteral("speakingEvalAiPromptBytes"),
            static_cast<double>(metrics.speakingEvalAiPromptBytes)
        },
        {
            QStringLiteral("speakingEvalAiResponseBytes"),
            static_cast<double>(metrics.speakingEvalAiResponseBytes)
        },
        {
            QStringLiteral("speakingEvalExportPdfBytes"),
            static_cast<double>(metrics.speakingEvalExportPdfBytes)
        },
        {
            QStringLiteral("speakingEvalExportArchiveBytes"),
            static_cast<double>(metrics.speakingEvalExportArchiveBytes)
        },
        {
            QStringLiteral("staffDirectoryNativeTextBytes"),
            static_cast<double>(metrics.staffDirectoryNativeTextBytes)
        },
        {
            QStringLiteral("staffDirectoryGsTextBytes"),
            static_cast<double>(metrics.staffDirectoryGsTextBytes)
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
        {
            QStringLiteral("classTransferRawBytesRetained"),
            metrics.classTransferRawBytesRetained
        },
        {
            QStringLiteral("classTransferJsonDocumentRetained"),
            metrics.classTransferJsonDocumentRetained
        },
        {
            QStringLiteral("classTransferPackageRetained"),
            metrics.classTransferPackageRetained
        },
        {
            QStringLiteral("classTransferPreviewRetained"),
            metrics.classTransferPreviewRetained
        },
        {
            QStringLiteral("classTransferDialogRetained"),
            metrics.classTransferDialogRetained
        },
        {
            QStringLiteral("classTransferOperationRetained"),
            metrics.classTransferOperationRetained
        },
        {
            QStringLiteral("speakingEvalReportListRetained"),
            metrics.speakingEvalReportListRetained
        },
        {
            QStringLiteral("speakingEvalReportDialogRetained"),
            metrics.speakingEvalReportDialogRetained
        },
        {
            QStringLiteral("speakingEvalExportDialogRetained"),
            metrics.speakingEvalExportDialogRetained
        },
        {
            QStringLiteral("speakingEvalAiDialogRetained"),
            metrics.speakingEvalAiDialogRetained
        },
        {
            QStringLiteral("speakingEvalAiResponseRetained"),
            metrics.speakingEvalAiResponseRetained
        },
        {
            QStringLiteral("speakingEvalExportOperationRetained"),
            metrics.speakingEvalExportOperationRetained
        },
        {
            QStringLiteral("speakingEvalOperationRetained"),
            metrics.speakingEvalOperationRetained
        },
        {
            QStringLiteral("staffDirectoryNativeTableRetained"),
            metrics.staffDirectoryNativeTableRetained
        },
        {
            QStringLiteral("staffDirectoryGsTableRetained"),
            metrics.staffDirectoryGsTableRetained
        },
        {
            QStringLiteral("staffDirectoryNativeOperationRetained"),
            metrics.staffDirectoryNativeOperationRetained
        },
        {
            QStringLiteral("staffDirectoryGsOperationRetained"),
            metrics.staffDirectoryGsOperationRetained
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
        {
            QStringLiteral("classTransferOperationsStarted"),
            static_cast<double>(metrics.classTransferOperationsStarted)
        },
        {
            QStringLiteral("classTransferPackagesLoaded"),
            static_cast<double>(metrics.classTransferPackagesLoaded)
        },
        {
            QStringLiteral("classTransferPreviewsPrepared"),
            static_cast<double>(metrics.classTransferPreviewsPrepared)
        },
        {
            QStringLiteral("classTransferOperationsApplied"),
            static_cast<double>(metrics.classTransferOperationsApplied)
        },
        {
            QStringLiteral("classTransferOperationsFailed"),
            static_cast<double>(metrics.classTransferOperationsFailed)
        },
        {
            QStringLiteral("classTransferOperationsReleased"),
            static_cast<double>(metrics.classTransferOperationsReleased)
        },
        {
            QStringLiteral("speakingEvalOperationsStarted"),
            static_cast<double>(metrics.speakingEvalOperationsStarted)
        },
        {
            QStringLiteral("speakingEvalReportBatchesPrepared"),
            static_cast<double>(metrics.speakingEvalReportBatchesPrepared)
        },
        {
            QStringLiteral("speakingEvalReportDialogsOpened"),
            static_cast<double>(metrics.speakingEvalReportDialogsOpened)
        },
        {
            QStringLiteral("speakingEvalExportDialogsOpened"),
            static_cast<double>(metrics.speakingEvalExportDialogsOpened)
        },
        {
            QStringLiteral("speakingEvalAiDialogsOpened"),
            static_cast<double>(metrics.speakingEvalAiDialogsOpened)
        },
        {
            QStringLiteral("speakingEvalExportsStarted"),
            static_cast<double>(metrics.speakingEvalExportsStarted)
        },
        {
            QStringLiteral("speakingEvalExportsCompleted"),
            static_cast<double>(metrics.speakingEvalExportsCompleted)
        },
        {
            QStringLiteral("speakingEvalOperationsFailed"),
            static_cast<double>(metrics.speakingEvalOperationsFailed)
        },
        {
            QStringLiteral("speakingEvalOperationsReleased"),
            static_cast<double>(metrics.speakingEvalOperationsReleased)
        },
        {
            QStringLiteral("staffDirectoryOperationsStarted"),
            static_cast<double>(metrics.staffDirectoryOperationsStarted)
        },
        {
            QStringLiteral("staffDirectoryPagesPrepared"),
            static_cast<double>(metrics.staffDirectoryPagesPrepared)
        },
        {
            QStringLiteral("staffDirectoryRefreshes"),
            static_cast<double>(metrics.staffDirectoryRefreshes)
        },
        {
            QStringLiteral("staffDirectoryLeaves"),
            static_cast<double>(metrics.staffDirectoryLeaves)
        },
        {
            QStringLiteral("staffDirectoryReentries"),
            static_cast<double>(metrics.staffDirectoryReentries)
        },
        {
            QStringLiteral("staffDirectoryOperationsFailed"),
            static_cast<double>(metrics.staffDirectoryOperationsFailed)
        },
        {
            QStringLiteral("staffDirectoryOperationsReleased"),
            static_cast<double>(metrics.staffDirectoryOperationsReleased)
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
        metrics.scheduleImportExistingTeacherCount = 0;
        metrics.scheduleImportExistingClassCount = 0;
        metrics.scheduleImportExistingClassInfoCount = 0;
        metrics.scheduleImportApplyFinalClassCount = 0;
        metrics.scheduleImportApplyFinalScheduleRowCount = 0;
        metrics.scheduleImportApplyTeacherResolutionCount = 0;
        metrics.scheduleImportApplyClassResolutionCount = 0;
        metrics.scheduleImportTeachersCreated = 0;
        metrics.scheduleImportTeachersUpdated = 0;
        metrics.scheduleImportClassesCreated = 0;
        metrics.scheduleImportClassesUpdated = 0;
        metrics.scheduleImportClassesSkipped = 0;
        metrics.scheduleImportSchedulesCleared = 0;
        metrics.scheduleImportIgnoredCells = 0;

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

void StartupProfiler::recordScheduleImportApplyInputs(
    int existingTeacherCount,
    int existingClassCount,
    int existingClassInfoCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.scheduleImportExistingTeacherCount =
            qMax(0, existingTeacherCount);
        metrics.scheduleImportExistingClassCount =
            qMax(0, existingClassCount);
        metrics.scheduleImportExistingClassInfoCount =
            qMax(0, existingClassInfoCount);
        const QString detail =
            QStringLiteral(
                "existingTeachers=%1; existingClasses=%2; existingClassInfo=%3"
                )
                .arg(metrics.scheduleImportExistingTeacherCount)
                .arg(metrics.scheduleImportExistingClassCount)
                .arg(metrics.scheduleImportExistingClassInfoCount);
        profiler->recordEvent(
            QStringLiteral("schedule-import-apply-inputs"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-apply-inputs %1")
                .arg(detail)
            );
    }
}

void StartupProfiler::recordScheduleImportApplyPrepared(
    int finalClassCount,
    int finalScheduleRowCount,
    int teacherResolutionCount,
    int classResolutionCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.scheduleImportApplyFinalClassCount =
            qMax(0, finalClassCount);
        metrics.scheduleImportApplyFinalScheduleRowCount =
            qMax(0, finalScheduleRowCount);
        metrics.scheduleImportApplyTeacherResolutionCount =
            qMax(0, teacherResolutionCount);
        metrics.scheduleImportApplyClassResolutionCount =
            qMax(0, classResolutionCount);
        const QString detail =
            QStringLiteral(
                "finalClasses=%1; finalScheduleRows=%2; teacherResolutions=%3; classResolutions=%4"
                )
                .arg(metrics.scheduleImportApplyFinalClassCount)
                .arg(metrics.scheduleImportApplyFinalScheduleRowCount)
                .arg(metrics.scheduleImportApplyTeacherResolutionCount)
                .arg(metrics.scheduleImportApplyClassResolutionCount);
        profiler->recordEvent(
            QStringLiteral("schedule-import-apply-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-apply-prepared %1")
                .arg(detail)
            );
    }
}

void StartupProfiler::recordScheduleImportApplied(
    int teachersCreated,
    int teachersUpdated,
    int classesCreated,
    int classesUpdated,
    int classesSkipped,
    int schedulesCleared,
    int ignoredCells
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.scheduleImportOperationsApplied;
        metrics.scheduleImportTeachersCreated = qMax(0, teachersCreated);
        metrics.scheduleImportTeachersUpdated = qMax(0, teachersUpdated);
        metrics.scheduleImportClassesCreated = qMax(0, classesCreated);
        metrics.scheduleImportClassesUpdated = qMax(0, classesUpdated);
        metrics.scheduleImportClassesSkipped = qMax(0, classesSkipped);
        metrics.scheduleImportSchedulesCleared = qMax(0, schedulesCleared);
        metrics.scheduleImportIgnoredCells = qMax(0, ignoredCells);
        const QString detail =
            QStringLiteral(
                "committed=true; teachersCreated=%1; teachersUpdated=%2; classesCreated=%3; classesUpdated=%4; classesSkipped=%5; schedulesCleared=%6; ignoredCells=%7"
                )
                .arg(metrics.scheduleImportTeachersCreated)
                .arg(metrics.scheduleImportTeachersUpdated)
                .arg(metrics.scheduleImportClassesCreated)
                .arg(metrics.scheduleImportClassesUpdated)
                .arg(metrics.scheduleImportClassesSkipped)
                .arg(metrics.scheduleImportSchedulesCleared)
                .arg(metrics.scheduleImportIgnoredCells);
        profiler->recordEvent(
            QStringLiteral("schedule-import-operation-applied"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("schedule-import-operation-applied %1")
                .arg(detail)
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

void StartupProfiler::recordClassTransferStarted(
    const QString& filePath,
    qint64 rawJsonBytes
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.classTransferOperationsStarted;
        metrics.classTransferRawJsonBytes = qMax<qint64>(0, rawJsonBytes);
        metrics.classTransferRawBytesRetained = true;
        metrics.classTransferJsonDocumentRetained = true;
        metrics.classTransferPackageRetained = false;
        metrics.classTransferPreviewRetained = false;
        metrics.classTransferDialogRetained = false;
        metrics.classTransferOperationRetained = true;

        const QString detail =
            QStringLiteral(
                "path=%1; rawJsonBytes=%2; rawBytesRetained=true; jsonDocumentRetained=true"
                )
                .arg(filePath)
                .arg(metrics.classTransferRawJsonBytes);
        profiler->recordEvent(
            QStringLiteral("class-transfer-operation-start"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("class-transfer-operation-start %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("class-transfer-operation-start"),
            detail
            );
    }
}

void StartupProfiler::recordClassTransferPackageLoaded(
    int teacherCount,
    int classCount,
    int rosterColumnCount,
    int rosterRowCount,
    int rosterCellCount,
    int evaluationCount,
    int evaluationRowCount,
    int evaluationCellCount,
    int scheduleRowCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.classTransferPackagesLoaded;
        metrics.classTransferRawBytesRetained = false;
        metrics.classTransferJsonDocumentRetained = false;
        metrics.classTransferPackageRetained = true;
        metrics.classTransferPackageTeacherCount = qMax(0, teacherCount);
        metrics.classTransferPackageClassCount = qMax(0, classCount);
        metrics.classTransferPackageRosterColumnCount = qMax(0, rosterColumnCount);
        metrics.classTransferPackageRosterRowCount = qMax(0, rosterRowCount);
        metrics.classTransferPackageRosterCellCount = qMax(0, rosterCellCount);
        metrics.classTransferPackageEvaluationCount = qMax(0, evaluationCount);
        metrics.classTransferPackageEvaluationRowCount = qMax(0, evaluationRowCount);
        metrics.classTransferPackageEvaluationCellCount = qMax(0, evaluationCellCount);
        metrics.classTransferPackageScheduleRowCount = qMax(0, scheduleRowCount);

        const QString detail =
            QStringLiteral(
                "teachers=%1; classes=%2; rosterColumns=%3; rosterRows=%4; rosterCells=%5; evaluations=%6; evaluationRows=%7; evaluationCells=%8; scheduleRows=%9; rawBytesRetained=false; jsonDocumentRetained=false; packageRetained=true"
                )
                .arg(metrics.classTransferPackageTeacherCount)
                .arg(metrics.classTransferPackageClassCount)
                .arg(metrics.classTransferPackageRosterColumnCount)
                .arg(metrics.classTransferPackageRosterRowCount)
                .arg(metrics.classTransferPackageRosterCellCount)
                .arg(metrics.classTransferPackageEvaluationCount)
                .arg(metrics.classTransferPackageEvaluationRowCount)
                .arg(metrics.classTransferPackageEvaluationCellCount)
                .arg(metrics.classTransferPackageScheduleRowCount);
        profiler->recordEvent(
            QStringLiteral("class-transfer-package-loaded"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("class-transfer-package-loaded %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("class-transfer-package-loaded"),
            detail
            );
    }
}

void StartupProfiler::recordClassTransferPreviewPrepared(
    int previewTeacherCount,
    int previewClassCount,
    int matchingTeacherCount,
    int matchingClassCount,
    int destinationTeacherCount,
    int destinationClassCount,
    int destinationClassInfoResultCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.classTransferPreviewsPrepared;
        metrics.classTransferPreviewRetained = true;
        metrics.classTransferPreviewTeacherCount = qMax(0, previewTeacherCount);
        metrics.classTransferPreviewClassCount = qMax(0, previewClassCount);
        metrics.classTransferMatchingTeacherCount = qMax(0, matchingTeacherCount);
        metrics.classTransferMatchingClassCount = qMax(0, matchingClassCount);
        metrics.classTransferDestinationTeacherCount = qMax(0, destinationTeacherCount);
        metrics.classTransferDestinationClassCount = qMax(0, destinationClassCount);
        metrics.classTransferDestinationClassInfoResultCount =
            qMax(0, destinationClassInfoResultCount);

        const QString detail =
            QStringLiteral(
                "previewTeachers=%1; previewClasses=%2; matchingTeachers=%3; matchingClasses=%4; destinationTeachers=%5; destinationClasses=%6; destinationClassInfoResults=%7; previewRetained=true"
                )
                .arg(metrics.classTransferPreviewTeacherCount)
                .arg(metrics.classTransferPreviewClassCount)
                .arg(metrics.classTransferMatchingTeacherCount)
                .arg(metrics.classTransferMatchingClassCount)
                .arg(metrics.classTransferDestinationTeacherCount)
                .arg(metrics.classTransferDestinationClassCount)
                .arg(metrics.classTransferDestinationClassInfoResultCount);
        profiler->recordEvent(
            QStringLiteral("class-transfer-preview-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("class-transfer-preview-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("class-transfer-preview-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordClassTransferDialogPrepared(
    int teacherControlCount,
    int classControlCount,
    int teacherResolutionCount,
    int classResolutionCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.classTransferDialogRetained = true;
        metrics.classTransferDialogTeacherControlCount = qMax(0, teacherControlCount);
        metrics.classTransferDialogClassControlCount = qMax(0, classControlCount);
        metrics.classTransferPlanTeacherResolutionCount = qMax(0, teacherResolutionCount);
        metrics.classTransferPlanClassResolutionCount = qMax(0, classResolutionCount);

        const QString detail =
            QStringLiteral(
                "teacherControls=%1; classControls=%2; teacherResolutions=%3; classResolutions=%4; dialogRetained=true"
                )
                .arg(metrics.classTransferDialogTeacherControlCount)
                .arg(metrics.classTransferDialogClassControlCount)
                .arg(metrics.classTransferPlanTeacherResolutionCount)
                .arg(metrics.classTransferPlanClassResolutionCount);
        profiler->recordEvent(
            QStringLiteral("class-transfer-dialog-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("class-transfer-dialog-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("class-transfer-dialog-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordClassTransferApplied(
    int teachersCreated,
    int teachersKept,
    int teachersReplaced,
    int classesCreated,
    int classesReplaced,
    int classesSkipped,
    int destinationClassesBefore,
    int destinationClassesAfter,
    int destinationTeachersBefore,
    int destinationTeachersAfter
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.classTransferOperationsApplied;
        metrics.classTransferTeachersCreated = qMax(0, teachersCreated);
        metrics.classTransferTeachersKept = qMax(0, teachersKept);
        metrics.classTransferTeachersReplaced = qMax(0, teachersReplaced);
        metrics.classTransferClassesCreated = qMax(0, classesCreated);
        metrics.classTransferClassesReplaced = qMax(0, classesReplaced);
        metrics.classTransferClassesSkipped = qMax(0, classesSkipped);
        metrics.classTransferDestinationClassesBefore = qMax(0, destinationClassesBefore);
        metrics.classTransferDestinationClassesAfter = qMax(0, destinationClassesAfter);
        metrics.classTransferDestinationTeachersBefore = qMax(0, destinationTeachersBefore);
        metrics.classTransferDestinationTeachersAfter = qMax(0, destinationTeachersAfter);

        const QString detail =
            QStringLiteral(
                "committed=true; teachersCreated=%1; teachersKept=%2; teachersReplaced=%3; classesCreated=%4; classesReplaced=%5; classesSkipped=%6; destinationClassesBefore=%7; destinationClassesAfter=%8; destinationTeachersBefore=%9; destinationTeachersAfter=%10"
                )
                .arg(metrics.classTransferTeachersCreated)
                .arg(metrics.classTransferTeachersKept)
                .arg(metrics.classTransferTeachersReplaced)
                .arg(metrics.classTransferClassesCreated)
                .arg(metrics.classTransferClassesReplaced)
                .arg(metrics.classTransferClassesSkipped)
                .arg(metrics.classTransferDestinationClassesBefore)
                .arg(metrics.classTransferDestinationClassesAfter)
                .arg(metrics.classTransferDestinationTeachersBefore)
                .arg(metrics.classTransferDestinationTeachersAfter);
        profiler->recordEvent(
            QStringLiteral("class-transfer-operation-applied"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("class-transfer-operation-applied %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("class-transfer-operation-applied"),
            detail
            );
    }
}

void StartupProfiler::recordClassTransferDialogReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->m_scheduleMetrics.classTransferDialogRetained = false;
        profiler->recordEvent(
            QStringLiteral("class-transfer-dialog-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("class-transfer-dialog-released")
            );
        profiler->checkpoint(
            QStringLiteral("class-transfer-dialog-released")
            );
    }
}

void StartupProfiler::recordClassTransferFailed(const QString& detail)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.classTransferOperationsFailed;
        profiler->recordEvent(
            QStringLiteral("class-transfer-operation-failed"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("class-transfer-operation-failed %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("class-transfer-operation-failed"),
            detail
            );
    }
}

void StartupProfiler::recordClassTransferOperationReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.classTransferOperationsReleased;
        metrics.classTransferRawBytesRetained = false;
        metrics.classTransferJsonDocumentRetained = false;
        metrics.classTransferPackageRetained = false;
        metrics.classTransferPreviewRetained = false;
        metrics.classTransferDialogRetained = false;
        metrics.classTransferOperationRetained = false;

        profiler->recordEvent(
            QStringLiteral("class-transfer-operation-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("class-transfer-operation-released")
            );
        profiler->checkpoint(
            QStringLiteral("class-transfer-operation-released")
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationOperationStarted()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.speakingEvalOperationsStarted;
        metrics.speakingEvalReportListRetained = false;
        metrics.speakingEvalReportDialogRetained = false;
        metrics.speakingEvalExportDialogRetained = false;
        metrics.speakingEvalAiDialogRetained = false;
        metrics.speakingEvalAiResponseRetained = false;
        metrics.speakingEvalExportOperationRetained = false;
        metrics.speakingEvalOperationRetained = true;

        const QString detail =
            QStringLiteral("operationRetained=true");
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-operation-start"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-operation-start %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-operation-start"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationPagePrepared(
    int sourceClassCount,
    int visibleClassCount,
    int classTabWidgetCount,
    int classTabCount,
    int evaluationTabCount,
    int modelRowCount,
    int modelColumnCount,
    int modelCellCount,
    int loadedEvaluationCount,
    int loadedEvaluationRowCount,
    int loadedEvaluationCellCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.speakingEvalSourceClassCount = qMax(0, sourceClassCount);
        metrics.speakingEvalVisibleClassCount = qMax(0, visibleClassCount);
        metrics.speakingEvalClassTabWidgetCount = qMax(0, classTabWidgetCount);
        metrics.speakingEvalClassTabCount = qMax(0, classTabCount);
        metrics.speakingEvalEvaluationTabCount = qMax(0, evaluationTabCount);
        metrics.speakingEvalModelRowCount = qMax(0, modelRowCount);
        metrics.speakingEvalModelColumnCount = qMax(0, modelColumnCount);
        metrics.speakingEvalModelCellCount = qMax(0, modelCellCount);
        metrics.speakingEvalLoadedEvaluationCount = qMax(0, loadedEvaluationCount);
        metrics.speakingEvalLoadedEvaluationRowCount =
            qMax(0, loadedEvaluationRowCount);
        metrics.speakingEvalLoadedEvaluationCellCount =
            qMax(0, loadedEvaluationCellCount);

        const QString detail =
            QStringLiteral(
                "sourceClasses=%1; visibleClasses=%2; classTabWidgets=%3; classTabs=%4; evaluationTabs=%5; modelRows=%6; modelColumns=%7; modelCells=%8; loadedEvaluations=%9; loadedRows=%10; loadedCells=%11"
                )
                .arg(metrics.speakingEvalSourceClassCount)
                .arg(metrics.speakingEvalVisibleClassCount)
                .arg(metrics.speakingEvalClassTabWidgetCount)
                .arg(metrics.speakingEvalClassTabCount)
                .arg(metrics.speakingEvalEvaluationTabCount)
                .arg(metrics.speakingEvalModelRowCount)
                .arg(metrics.speakingEvalModelColumnCount)
                .arg(metrics.speakingEvalModelCellCount)
                .arg(metrics.speakingEvalLoadedEvaluationCount)
                .arg(metrics.speakingEvalLoadedEvaluationRowCount)
                .arg(metrics.speakingEvalLoadedEvaluationCellCount);
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-page-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-page-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-page-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationReportsPrepared(
    int reportCount,
    qint64 reportTextBytes
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.speakingEvalReportBatchesPrepared;
        metrics.speakingEvalBatchReportCount = qMax(0, reportCount);
        metrics.speakingEvalBatchReportTextBytes = qMax<qint64>(0, reportTextBytes);
        metrics.speakingEvalReportListRetained = true;

        const QString detail =
            QStringLiteral(
                "reports=%1; reportTextBytes=%2; reportListRetained=true"
                )
                .arg(metrics.speakingEvalBatchReportCount)
                .arg(metrics.speakingEvalBatchReportTextBytes);
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-reports-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-reports-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-reports-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationReportDialogPrepared(
    int reportCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.speakingEvalReportDialogsOpened;
        metrics.speakingEvalReportDialogReportCount = qMax(0, reportCount);
        metrics.speakingEvalReportDialogRetained = true;

        const QString detail =
            QStringLiteral(
                "reports=%1; reportDialogRetained=true"
                )
                .arg(metrics.speakingEvalReportDialogReportCount);
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-report-dialog-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-report-dialog-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-report-dialog-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationReportDialogReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->m_scheduleMetrics.speakingEvalReportDialogRetained = false;
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-report-dialog-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-report-dialog-released")
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-report-dialog-released")
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationExportDialogPrepared(
    int reportCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.speakingEvalExportDialogsOpened;
        profiler->m_scheduleMetrics.speakingEvalExportDialogReportCount =
            qMax(0, reportCount);
        profiler->m_scheduleMetrics.speakingEvalExportDialogRetained = true;
        const QString detail =
            QStringLiteral(
                "reports=%1; exportDialogRetained=true"
                )
                .arg(profiler->m_scheduleMetrics.speakingEvalExportDialogReportCount);
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-export-dialog-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-export-dialog-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-export-dialog-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationExportDialogReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        profiler->m_scheduleMetrics.speakingEvalExportDialogRetained = false;
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-export-dialog-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-export-dialog-released")
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-export-dialog-released")
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationAiDialogPrepared(
    int reportCount,
    int selectionRowCount,
    int selectionColumnCount,
    int selectionItemCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.speakingEvalAiDialogsOpened;
        metrics.speakingEvalAiDialogReportCount = qMax(0, reportCount);
        metrics.speakingEvalAiSelectionRowCount = qMax(0, selectionRowCount);
        metrics.speakingEvalAiSelectionColumnCount = qMax(0, selectionColumnCount);
        metrics.speakingEvalAiSelectionItemCount = qMax(0, selectionItemCount);
        metrics.speakingEvalAiDialogRetained = true;

        const QString detail =
            QStringLiteral(
                "reports=%1; selectionRows=%2; selectionColumns=%3; selectionItems=%4; aiDialogRetained=true"
                )
                .arg(metrics.speakingEvalAiDialogReportCount)
                .arg(metrics.speakingEvalAiSelectionRowCount)
                .arg(metrics.speakingEvalAiSelectionColumnCount)
                .arg(metrics.speakingEvalAiSelectionItemCount);
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-ai-dialog-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-ai-dialog-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-ai-dialog-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationAiResponsePrepared(
    qint64 promptBytes,
    qint64 responseBytes,
    int reviewRowCount,
    int reviewColumnCount,
    int reviewItemCount,
    int acceptedCommentCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.speakingEvalAiPromptBytes = qMax<qint64>(0, promptBytes);
        metrics.speakingEvalAiResponseBytes = qMax<qint64>(0, responseBytes);
        metrics.speakingEvalAiReviewRowCount = qMax(0, reviewRowCount);
        metrics.speakingEvalAiReviewColumnCount = qMax(0, reviewColumnCount);
        metrics.speakingEvalAiReviewItemCount = qMax(0, reviewItemCount);
        metrics.speakingEvalAiAcceptedCommentCount = qMax(0, acceptedCommentCount);
        metrics.speakingEvalAiResponseRetained = true;

        const QString detail =
            QStringLiteral(
                "promptBytes=%1; responseBytes=%2; reviewRows=%3; reviewColumns=%4; reviewItems=%5; acceptedComments=%6; aiResponseRetained=true"
                )
                .arg(metrics.speakingEvalAiPromptBytes)
                .arg(metrics.speakingEvalAiResponseBytes)
                .arg(metrics.speakingEvalAiReviewRowCount)
                .arg(metrics.speakingEvalAiReviewColumnCount)
                .arg(metrics.speakingEvalAiReviewItemCount)
                .arg(metrics.speakingEvalAiAcceptedCommentCount);
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-ai-response-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-ai-response-prepared %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-ai-response-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationAiDialogReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        metrics.speakingEvalAiDialogRetained = false;
        metrics.speakingEvalAiResponseRetained = false;
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-ai-dialog-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-ai-dialog-released")
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-ai-dialog-released")
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationExportStarted(
    int reportCount
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.speakingEvalExportsStarted;
        metrics.speakingEvalExportDialogReportCount = qMax(0, reportCount);
        metrics.speakingEvalExportOperationRetained = true;
        const QString detail =
            QStringLiteral(
                "reports=%1; exportOperationRetained=true"
                )
                .arg(metrics.speakingEvalExportDialogReportCount);
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-export-start"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-export-start %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-export-start"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationExportCompleted(
    int pdfCount,
    qint64 pdfBytes,
    qint64 archiveBytes
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.speakingEvalExportsCompleted;
        metrics.speakingEvalExportPdfCount = qMax(0, pdfCount);
        metrics.speakingEvalExportPdfBytes = qMax<qint64>(0, pdfBytes);
        metrics.speakingEvalExportArchiveBytes = qMax<qint64>(0, archiveBytes);
        metrics.speakingEvalExportOperationRetained = false;
        const QString detail =
            QStringLiteral(
                "pdfCount=%1; pdfBytes=%2; archiveBytes=%3; exportOperationRetained=false"
                )
                .arg(metrics.speakingEvalExportPdfCount)
                .arg(metrics.speakingEvalExportPdfBytes)
                .arg(metrics.speakingEvalExportArchiveBytes);
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-export-complete"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-export-complete %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-export-complete"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationFailed(const QString& detail)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.speakingEvalOperationsFailed;
        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-operation-failed"),
            detail
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-operation-failed %1")
                .arg(detail)
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-operation-failed"),
            detail
            );
    }
}

void StartupProfiler::recordSpeakingEvaluationOperationReleased()
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.speakingEvalOperationsReleased;
        metrics.speakingEvalReportListRetained = false;
        metrics.speakingEvalReportDialogRetained = false;
        metrics.speakingEvalExportDialogRetained = false;
        metrics.speakingEvalAiDialogRetained = false;
        metrics.speakingEvalAiResponseRetained = false;
        metrics.speakingEvalExportOperationRetained = false;
        metrics.speakingEvalOperationRetained = false;

        profiler->recordEvent(
            QStringLiteral("speaking-evaluation-operation-released")
            );
        appendProfilerWorkflowTrace(
            QStringLiteral("speaking-evaluation-operation-released")
            );
        profiler->checkpoint(
            QStringLiteral("speaking-evaluation-operation-released")
            );
    }
}

void StartupProfiler::recordStaffDirectoryOperationStarted(
    bool nativeDirectory
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.staffDirectoryOperationsStarted;
        const QString prefix = nativeDirectory
            ? QStringLiteral("staff-directory-native")
            : QStringLiteral("staff-directory-gs");
        if (nativeDirectory)
        {
            metrics.staffDirectoryNativeOperationRetained = true;
        }
        else
        {
            metrics.staffDirectoryGsOperationRetained = true;
        }

        const QString detail =
            QStringLiteral("operationRetained=true; native=%1")
                .arg(nativeDirectory ? QStringLiteral("true")
                                      : QStringLiteral("false"));
        profiler->recordEvent(
            prefix + QStringLiteral("-operation-start"),
            detail
            );
        appendProfilerWorkflowTrace(
            prefix + QStringLiteral("-operation-start %1").arg(detail)
            );
        profiler->checkpoint(
            prefix + QStringLiteral("-operation-start"),
            detail
            );
    }
}

void StartupProfiler::recordStaffDirectoryPagePrepared(
    bool nativeDirectory,
    int rowCount,
    int columnCount,
    int itemCount,
    int pageWidgetCount,
    qint64 textBytes
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.staffDirectoryPagesPrepared;
        const QString prefix = nativeDirectory
            ? QStringLiteral("staff-directory-native")
            : QStringLiteral("staff-directory-gs");
        if (nativeDirectory)
        {
            metrics.staffDirectoryNativeRowCount = qMax(0, rowCount);
            metrics.staffDirectoryNativeColumnCount = qMax(0, columnCount);
            metrics.staffDirectoryNativeItemCount = qMax(0, itemCount);
            metrics.staffDirectoryNativePageWidgetCount =
                qMax(0, pageWidgetCount);
            metrics.staffDirectoryNativeTextBytes = qMax<qint64>(0, textBytes);
            metrics.staffDirectoryNativeTableRetained = true;
        }
        else
        {
            metrics.staffDirectoryGsRowCount = qMax(0, rowCount);
            metrics.staffDirectoryGsColumnCount = qMax(0, columnCount);
            metrics.staffDirectoryGsItemCount = qMax(0, itemCount);
            metrics.staffDirectoryGsPageWidgetCount = qMax(0, pageWidgetCount);
            metrics.staffDirectoryGsTextBytes = qMax<qint64>(0, textBytes);
            metrics.staffDirectoryGsTableRetained = true;
        }

        const QString detail =
            QStringLiteral(
                "rows=%1; columns=%2; items=%3; pageWidgets=%4; textBytes=%5; tableRetained=true"
                )
                .arg(qMax(0, rowCount))
                .arg(qMax(0, columnCount))
                .arg(qMax(0, itemCount))
                .arg(qMax(0, pageWidgetCount))
                .arg(qMax<qint64>(0, textBytes));
        profiler->recordEvent(
            prefix + QStringLiteral("-page-prepared"),
            detail
            );
        appendProfilerWorkflowTrace(
            prefix + QStringLiteral("-page-prepared %1").arg(detail)
            );
        profiler->checkpoint(
            prefix + QStringLiteral("-page-prepared"),
            detail
            );
    }
}

void StartupProfiler::recordStaffDirectoryRefreshed(
    bool nativeDirectory,
    int refreshIndex,
    int rowCount,
    int columnCount,
    int itemCount,
    int pageWidgetCount,
    qint64 textBytes
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.staffDirectoryRefreshes;
        const QString prefix = nativeDirectory
            ? QStringLiteral("staff-directory-native")
            : QStringLiteral("staff-directory-gs");
        if (nativeDirectory)
        {
            metrics.staffDirectoryNativeRowCount = qMax(0, rowCount);
            metrics.staffDirectoryNativeColumnCount = qMax(0, columnCount);
            metrics.staffDirectoryNativeItemCount = qMax(0, itemCount);
            metrics.staffDirectoryNativePageWidgetCount =
                qMax(0, pageWidgetCount);
            metrics.staffDirectoryNativeTextBytes = qMax<qint64>(0, textBytes);
            metrics.staffDirectoryNativeRefreshCount = qMax(0, refreshIndex);
            metrics.staffDirectoryNativeTableRetained = true;
        }
        else
        {
            metrics.staffDirectoryGsRowCount = qMax(0, rowCount);
            metrics.staffDirectoryGsColumnCount = qMax(0, columnCount);
            metrics.staffDirectoryGsItemCount = qMax(0, itemCount);
            metrics.staffDirectoryGsPageWidgetCount = qMax(0, pageWidgetCount);
            metrics.staffDirectoryGsTextBytes = qMax<qint64>(0, textBytes);
            metrics.staffDirectoryGsRefreshCount = qMax(0, refreshIndex);
            metrics.staffDirectoryGsTableRetained = true;
        }

        const QString detail =
            QStringLiteral(
                "refresh=%1; rows=%2; columns=%3; items=%4; pageWidgets=%5; textBytes=%6; tableRetained=true"
                )
                .arg(qMax(0, refreshIndex))
                .arg(qMax(0, rowCount))
                .arg(qMax(0, columnCount))
                .arg(qMax(0, itemCount))
                .arg(qMax(0, pageWidgetCount))
                .arg(qMax<qint64>(0, textBytes));
        const QString checkpointName =
            prefix + QStringLiteral("-refresh-%1").arg(qMax(0, refreshIndex));
        profiler->recordEvent(checkpointName, detail);
        appendProfilerWorkflowTrace(
            checkpointName + QStringLiteral(" %1").arg(detail)
            );
        profiler->checkpoint(checkpointName, detail);
    }
}

void StartupProfiler::recordStaffDirectoryLeft(bool nativeDirectory)
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.staffDirectoryLeaves;
        const QString prefix = nativeDirectory
            ? QStringLiteral("staff-directory-native")
            : QStringLiteral("staff-directory-gs");
        const QString detail =
            QStringLiteral("tableRetained=true; operationRetained=true");
        profiler->recordEvent(
            prefix + QStringLiteral("-page-left"),
            detail
            );
        appendProfilerWorkflowTrace(
            prefix + QStringLiteral("-page-left %1").arg(detail)
            );
        profiler->checkpoint(
            prefix + QStringLiteral("-page-left"),
            detail
            );
    }
}

void StartupProfiler::recordStaffDirectoryReentered(
    bool nativeDirectory,
    int rowCount,
    int columnCount,
    int itemCount,
    int pageWidgetCount,
    qint64 textBytes
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        StartupApplicationMetrics& metrics = profiler->m_scheduleMetrics;
        ++metrics.staffDirectoryReentries;
        const QString prefix = nativeDirectory
            ? QStringLiteral("staff-directory-native")
            : QStringLiteral("staff-directory-gs");
        if (nativeDirectory)
        {
            metrics.staffDirectoryNativeRowCount = qMax(0, rowCount);
            metrics.staffDirectoryNativeColumnCount = qMax(0, columnCount);
            metrics.staffDirectoryNativeItemCount = qMax(0, itemCount);
            metrics.staffDirectoryNativePageWidgetCount =
                qMax(0, pageWidgetCount);
            metrics.staffDirectoryNativeTextBytes = qMax<qint64>(0, textBytes);
            ++metrics.staffDirectoryNativeReentryCount;
            metrics.staffDirectoryNativeTableRetained = true;
        }
        else
        {
            metrics.staffDirectoryGsRowCount = qMax(0, rowCount);
            metrics.staffDirectoryGsColumnCount = qMax(0, columnCount);
            metrics.staffDirectoryGsItemCount = qMax(0, itemCount);
            metrics.staffDirectoryGsPageWidgetCount = qMax(0, pageWidgetCount);
            metrics.staffDirectoryGsTextBytes = qMax<qint64>(0, textBytes);
            ++metrics.staffDirectoryGsReentryCount;
            metrics.staffDirectoryGsTableRetained = true;
        }

        const QString detail =
            QStringLiteral(
                "rows=%1; columns=%2; items=%3; pageWidgets=%4; textBytes=%5; tableRetained=true"
                )
                .arg(qMax(0, rowCount))
                .arg(qMax(0, columnCount))
                .arg(qMax(0, itemCount))
                .arg(qMax(0, pageWidgetCount))
                .arg(qMax<qint64>(0, textBytes));
        profiler->recordEvent(
            prefix + QStringLiteral("-page-reentered"),
            detail
            );
        appendProfilerWorkflowTrace(
            prefix + QStringLiteral("-page-reentered %1").arg(detail)
            );
        profiler->checkpoint(
            prefix + QStringLiteral("-page-reentered"),
            detail
            );
    }
}

void StartupProfiler::recordStaffDirectoryFailed(
    bool nativeDirectory,
    const QString& detail
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.staffDirectoryOperationsFailed;
        const QString prefix = nativeDirectory
            ? QStringLiteral("staff-directory-native")
            : QStringLiteral("staff-directory-gs");
        profiler->recordEvent(
            prefix + QStringLiteral("-operation-failed"),
            detail
            );
        appendProfilerWorkflowTrace(
            prefix + QStringLiteral("-operation-failed %1").arg(detail)
            );
        profiler->checkpoint(
            prefix + QStringLiteral("-operation-failed"),
            detail
            );
    }
}

void StartupProfiler::recordStaffDirectoryOperationReleased(
    bool nativeDirectory
    )
{
    if (StartupProfiler* profiler = activeProfiler())
    {
        ++profiler->m_scheduleMetrics.staffDirectoryOperationsReleased;
        const QString prefix = nativeDirectory
            ? QStringLiteral("staff-directory-native")
            : QStringLiteral("staff-directory-gs");
        if (nativeDirectory)
        {
            profiler->m_scheduleMetrics.staffDirectoryNativeOperationRetained =
                false;
        }
        else
        {
            profiler->m_scheduleMetrics.staffDirectoryGsOperationRetained =
                false;
        }

        const QString detail =
            QStringLiteral("operationRetained=false; tableRetained=true");
        profiler->recordEvent(
            prefix + QStringLiteral("-operation-released"),
            detail
            );
        appendProfilerWorkflowTrace(
            prefix + QStringLiteral("-operation-released %1").arg(detail)
            );
        profiler->checkpoint(
            prefix + QStringLiteral("-operation-released"),
            detail
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

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
    int subPrepClassInformationWidgetCount = 0;
    int subPrepClassInformationTextEditCount = 0;
    int subPrepClassInformationNavigationRowCount = 0;
    int subPrepClassInformationSourceClassCount = 0;
    int subPrepClassInformationVisibleClassCount = 0;
    int subPrepClassInformationGroupCount = 0;
    int subPrepClassInformationClassInfoLookupCount = 0;
    int subPrepClassInformationTeacherLookupCount = 0;
    int subPrepClassInformationRosterLookupCount = 0;
    int subPrepClassInformationClassQueryCount = 0;
    int subPrepClassInformationClassResultRowCount = 0;
    int subPrepClassInformationClassInfoQueryCount = 0;
    int subPrepClassInformationClassInfoResultRowCount = 0;
    int subPrepClassInformationClassInfoScheduleRowCount = 0;
    int subPrepClassInformationTeacherQueryCount = 0;
    int subPrepClassInformationTeacherResultRowCount = 0;
    int subPrepClassInformationRosterQueryCount = 0;
    int subPrepClassInformationRosterResultRowCount = 0;
    int subPrepClassInformationRosterStudentResultCount = 0;
    int subPrepClassInformationRebuildCount = 0;
    int subPrepSelectedClassId = -1;
    int classesSourceClassCount = 0;
    int classesVisibleClassCount = 0;
    int classesNavigationGradeGroupCount = 0;
    int classesNavigationClassTabCount = 0;
    int classesNavigationWidgetCount = 0;
    int classesClassQueryCount = 0;
    int classesClassResultRowCount = 0;
    int classesClassInfoQueryCount = 0;
    int classesClassInfoResultRowCount = 0;
    int classesClassInfoScheduleRowCount = 0;
    int classesTeacherQueryCount = 0;
    int classesTeacherResultRowCount = 0;
    int classesVisibleSectionCount = 0;
    int classesInstantiatedEditorCount = 0;
    int classesLoadedEditorClassCount = 0;
    int classesRebuildCount = 0;
    int classesSelectedClassId = -1;
    int scheduleModelRowCount = 0;
    int scheduleModelCellCount = 0;
    int scheduleModelEntryCount = 0;
    int scheduleTableRowCount = 0;
    int scheduleTableColumnCount = 0;
    int scheduleTableItemCount = 0;
    int scheduleTableCellWidgetCount = 0;
    int scheduleVisibleClassCount = 0;
    int scheduleImportWorkbookSheetCount = 0;
    int scheduleImportWorkbookUserCount = 0;
    int scheduleImportWorkbookClassCandidateCount = 0;
    int scheduleImportWorkbookDiagnosticCount = 0;
    int scheduleImportPreviewTeacherCount = 0;
    int scheduleImportPreviewClassCount = 0;
    int scheduleImportPreviewDiagnosticCount = 0;
    int scheduleImportReviewTeacherControlCount = 0;
    int scheduleImportReviewClassControlCount = 0;
    int scheduleImportReviewPreviewEntryCount = 0;
    int scheduleImportExistingTeacherCount = 0;
    int scheduleImportExistingClassCount = 0;
    int scheduleImportExistingClassInfoCount = 0;
    int scheduleImportApplyFinalClassCount = 0;
    int scheduleImportApplyFinalScheduleRowCount = 0;
    int scheduleImportApplyTeacherResolutionCount = 0;
    int scheduleImportApplyClassResolutionCount = 0;
    int scheduleImportTeachersCreated = 0;
    int scheduleImportTeachersUpdated = 0;
    int scheduleImportClassesCreated = 0;
    int scheduleImportClassesUpdated = 0;
    int scheduleImportClassesSkipped = 0;
    int scheduleImportSchedulesCleared = 0;
    int scheduleImportIgnoredCells = 0;
    int calendarCacheEventCount = 0;
    int calendarCacheDateBucketCount = 0;
    int calendarCacheLoadedRangeCount = 0;
    int calendarCacheRetainedRangeCount = 0;
    int calendarLoadedMonthCount = 0;
    int calendarOnDemandRetainedRangeCount = 0;
    int calendarModelRevision = 0;
    int calendarPageWidgetCount = 0;
    int calendarViewObjectCount = 0;
    bool calendarCacheLoading = false;
    int calendarImportWorkbookSheetCount = 0;
    int calendarImportWorkbookCellCount = 0;
    int calendarImportWorkbookMergedRangeCount = 0;
    int calendarImportWorkbookSharedStringCount = 0;
    int calendarImportWorkbookStyleCount = 0;
    int calendarImportParsedEventCount = 0;
    int calendarImportParsedSkippedCount = 0;
    int calendarImportExistingEventCount = 0;
    int calendarImportEventsToSaveCount = 0;
    int calendarImportSavedEventCount = 0;
    int classTransferPackageTeacherCount = 0;
    int classTransferPackageClassCount = 0;
    int classTransferPackageRosterColumnCount = 0;
    int classTransferPackageRosterRowCount = 0;
    int classTransferPackageRosterCellCount = 0;
    int classTransferPackageEvaluationCount = 0;
    int classTransferPackageEvaluationRowCount = 0;
    int classTransferPackageEvaluationCellCount = 0;
    int classTransferPackageScheduleRowCount = 0;
    int classTransferPreviewTeacherCount = 0;
    int classTransferPreviewClassCount = 0;
    int classTransferMatchingTeacherCount = 0;
    int classTransferMatchingClassCount = 0;
    int classTransferDestinationTeacherCount = 0;
    int classTransferDestinationClassCount = 0;
    int classTransferDestinationClassInfoResultCount = 0;
    int classTransferDialogTeacherControlCount = 0;
    int classTransferDialogClassControlCount = 0;
    int classTransferPlanTeacherResolutionCount = 0;
    int classTransferPlanClassResolutionCount = 0;
    int classTransferTeachersCreated = 0;
    int classTransferTeachersKept = 0;
    int classTransferTeachersReplaced = 0;
    int classTransferClassesCreated = 0;
    int classTransferClassesReplaced = 0;
    int classTransferClassesSkipped = 0;
    int classTransferDestinationClassesBefore = 0;
    int classTransferDestinationClassesAfter = 0;
    int classTransferDestinationTeachersBefore = 0;
    int classTransferDestinationTeachersAfter = 0;
    int speakingEvalSourceClassCount = 0;
    int speakingEvalVisibleClassCount = 0;
    int speakingEvalClassTabWidgetCount = 0;
    int speakingEvalClassTabCount = 0;
    int speakingEvalEvaluationTabCount = 0;
    int speakingEvalModelRowCount = 0;
    int speakingEvalModelColumnCount = 0;
    int speakingEvalModelCellCount = 0;
    int speakingEvalLoadedEvaluationCount = 0;
    int speakingEvalLoadedEvaluationRowCount = 0;
    int speakingEvalLoadedEvaluationCellCount = 0;
    int speakingEvalBatchReportCount = 0;
    int speakingEvalReportDialogReportCount = 0;
    int speakingEvalExportDialogReportCount = 0;
    int speakingEvalAiDialogReportCount = 0;
    int speakingEvalAiSelectionRowCount = 0;
    int speakingEvalAiSelectionColumnCount = 0;
    int speakingEvalAiSelectionItemCount = 0;
    int speakingEvalAiReviewRowCount = 0;
    int speakingEvalAiReviewColumnCount = 0;
    int speakingEvalAiReviewItemCount = 0;
    int speakingEvalAiAcceptedCommentCount = 0;
    int speakingEvalExportPdfCount = 0;
    qint64 scheduleImportRawWorkbookBytes = 0;
    qint64 calendarImportRawWorkbookBytes = 0;
    qint64 classTransferRawJsonBytes = 0;
    qint64 speakingEvalBatchReportTextBytes = 0;
    qint64 speakingEvalAiPromptBytes = 0;
    qint64 speakingEvalAiResponseBytes = 0;
    qint64 speakingEvalExportPdfBytes = 0;
    qint64 speakingEvalExportArchiveBytes = 0;
    bool scheduleImportRawBytesRetained = false;
    bool scheduleImportWorkbookRetained = false;
    bool scheduleImportReviewRetained = false;
    bool calendarImportRawBytesRetained = false;
    bool calendarImportWorkbookRetained = false;
    bool calendarImportEventsRetained = false;
    bool calendarImportOperationRetained = false;
    bool classTransferRawBytesRetained = false;
    bool classTransferJsonDocumentRetained = false;
    bool classTransferPackageRetained = false;
    bool classTransferPreviewRetained = false;
    bool classTransferDialogRetained = false;
    bool classTransferOperationRetained = false;
    bool speakingEvalReportListRetained = false;
    bool speakingEvalReportDialogRetained = false;
    bool speakingEvalExportDialogRetained = false;
    bool speakingEvalAiDialogRetained = false;
    bool speakingEvalAiResponseRetained = false;
    bool speakingEvalExportOperationRetained = false;
    bool speakingEvalOperationRetained = false;
    int liveScheduleWidgetCount = 0;
    int livePdfDocumentCount = 0;
    quint64 scheduleWidgetsCreated = 0;
    quint64 scheduleRenderCount = 0;
    quint64 scheduleTableItemsCreated = 0;
    quint64 scheduleCellWidgetsCreated = 0;
    quint64 scheduleCellWidgetsRemoved = 0;
    quint64 scheduleCellWidgetsQueuedForDeletion = 0;
    quint64 scheduleImportOperationsStarted = 0;
    quint64 scheduleImportWorkbooksLoaded = 0;
    quint64 scheduleImportReviewsPrepared = 0;
    quint64 scheduleImportReviewsReleased = 0;
    quint64 scheduleImportOperationsCancelled = 0;
    quint64 scheduleImportOperationsApplied = 0;
    quint64 scheduleImportOperationsReleased = 0;
    quint64 calendarImportOperationsStarted = 0;
    quint64 calendarImportResponsesReceived = 0;
    quint64 calendarImportWorkbooksParsed = 0;
    quint64 calendarImportOperationsApplied = 0;
    quint64 calendarImportOperationsFailed = 0;
    quint64 calendarImportOperationsReleased = 0;
    quint64 classTransferOperationsStarted = 0;
    quint64 classTransferPackagesLoaded = 0;
    quint64 classTransferPreviewsPrepared = 0;
    quint64 classTransferOperationsApplied = 0;
    quint64 classTransferOperationsFailed = 0;
    quint64 classTransferOperationsReleased = 0;
    quint64 speakingEvalOperationsStarted = 0;
    quint64 speakingEvalReportBatchesPrepared = 0;
    quint64 speakingEvalReportDialogsOpened = 0;
    quint64 speakingEvalExportDialogsOpened = 0;
    quint64 speakingEvalAiDialogsOpened = 0;
    quint64 speakingEvalExportsStarted = 0;
    quint64 speakingEvalExportsCompleted = 0;
    quint64 speakingEvalOperationsFailed = 0;
    quint64 speakingEvalOperationsReleased = 0;
    quint64 pdfDocumentsLoaded = 0;
    quint64 pdfDocumentsReleased = 0;
    quint64 pdfRenderCount = 0;
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
    static void recordPageEntered(const QString& pageIdentifier);
    static void recordPageLeft(const QString& pageIdentifier);
    static void recordSubPrepClassInformationLifecycle(
        const QString& phase,
        const QString& detail = QString()
        );
    static void recordSubPrepRosterQuery(
        int classId,
        int columnCount,
        int rowCount,
        int cellCount,
        int returnedStudentCount
        );
    static void setSubPrepDiagnosticsActive(bool active);
    static void recordPdfDocumentLoaded(
        const QString& filePath,
        int pageCount
        );
    static void recordPdfDocumentReleased(const QString& filePath);
    static void recordPdfDocumentRendered(
        const QString& filePath,
        int width,
        int height
        );
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
        int cellWidgetsQueuedForDeletion,
        bool fullRender
        );
    static void recordScheduleImportStarted(
        const QString& filePath,
        qint64 rawWorkbookBytes
        );
    static void recordScheduleImportWorkbookLoaded(
        int sheetCount,
        int userCount,
        int classCandidateCount,
        int diagnosticCount
        );
    static void recordScheduleImportReviewPrepared(
        int teacherCount,
        int classCount,
        int diagnosticCount,
        int previewEntryCount,
        int teacherControlCount,
        int classControlCount
        );
    static void recordScheduleImportReviewReleased();
    static void recordScheduleImportCancelled();
    static void recordScheduleImportApplyInputs(
        int existingTeacherCount,
        int existingClassCount,
        int existingClassInfoCount
        );
    static void recordScheduleImportApplyPrepared(
        int finalClassCount,
        int finalScheduleRowCount,
        int teacherResolutionCount,
        int classResolutionCount
        );
    static void recordScheduleImportApplied(
        int teachersCreated,
        int teachersUpdated,
        int classesCreated,
        int classesUpdated,
        int classesSkipped,
        int schedulesCleared,
        int ignoredCells
        );
    static void recordScheduleImportOperationReleased();
    static void recordCalendarImportStarted(
        const QString& sourceUrl
        );
    static void recordCalendarImportResponseReceived(
        qint64 bytes
        );
    static void recordCalendarImportWorkbookParsed(
        int sheetCount,
        int cellCount,
        int mergedRangeCount,
        int sharedStringCount,
        int styleCount
        );
    static void recordCalendarImportEventsPrepared(
        int eventCount,
        int skippedCount
        );
    static void recordCalendarImportExistingEventsLoaded(
        int eventCount
        );
    static void recordCalendarImportSavePrepared(
        int eventsToSaveCount,
        int skippedCount
        );
    static void recordCalendarImportApplied(
        int savedCount,
        int skippedCount
        );
    static void recordCalendarImportFailed(
        const QString& detail
        );
    static void recordCalendarImportOperationReleased();
    static void recordClassTransferStarted(
        const QString& filePath,
        qint64 rawJsonBytes
        );
    static void recordClassTransferPackageLoaded(
        int teacherCount,
        int classCount,
        int rosterColumnCount,
        int rosterRowCount,
        int rosterCellCount,
        int evaluationCount,
        int evaluationRowCount,
        int evaluationCellCount,
        int scheduleRowCount
        );
    static void recordClassTransferPreviewPrepared(
        int previewTeacherCount,
        int previewClassCount,
        int matchingTeacherCount,
        int matchingClassCount,
        int destinationTeacherCount,
        int destinationClassCount,
        int destinationClassInfoResultCount
        );
    static void recordClassTransferDialogPrepared(
        int teacherControlCount,
        int classControlCount,
        int teacherResolutionCount,
        int classResolutionCount
        );
    static void recordClassTransferApplied(
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
        );
    static void recordClassTransferDialogReleased();
    static void recordClassTransferFailed(const QString& detail);
    static void recordClassTransferOperationReleased();
    static void recordSpeakingEvaluationOperationStarted();
    static void recordSpeakingEvaluationPagePrepared(
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
        );
    static void recordSpeakingEvaluationReportsPrepared(
        int reportCount,
        qint64 reportTextBytes
        );
    static void recordSpeakingEvaluationReportDialogPrepared(
        int reportCount
        );
    static void recordSpeakingEvaluationReportDialogReleased();
    static void recordSpeakingEvaluationExportDialogPrepared(
        int reportCount
        );
    static void recordSpeakingEvaluationExportDialogReleased();
    static void recordSpeakingEvaluationAiDialogPrepared(
        int reportCount,
        int selectionRowCount,
        int selectionColumnCount,
        int selectionItemCount
        );
    static void recordSpeakingEvaluationAiResponsePrepared(
        qint64 promptBytes,
        qint64 responseBytes,
        int reviewRowCount,
        int reviewColumnCount,
        int reviewItemCount,
        int acceptedCommentCount
        );
    static void recordSpeakingEvaluationAiDialogReleased();
    static void recordSpeakingEvaluationExportStarted(
        int reportCount
        );
    static void recordSpeakingEvaluationExportCompleted(
        int pdfCount,
        qint64 pdfBytes,
        qint64 archiveBytes
        );
    static void recordSpeakingEvaluationFailed(const QString& detail);
    static void recordSpeakingEvaluationOperationReleased();

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
    bool m_subPrepDiagnosticsActive = false;
};

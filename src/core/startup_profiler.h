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
    int liveScheduleWidgetCount = 0;
    int livePdfDocumentCount = 0;
    quint64 scheduleWidgetsCreated = 0;
    quint64 scheduleRenderCount = 0;
    quint64 scheduleTableItemsCreated = 0;
    quint64 scheduleCellWidgetsCreated = 0;
    quint64 scheduleCellWidgetsRemoved = 0;
    quint64 scheduleCellWidgetsQueuedForDeletion = 0;
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

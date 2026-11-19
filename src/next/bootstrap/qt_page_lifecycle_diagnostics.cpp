#include "next/bootstrap/qt_page_lifecycle_diagnostics.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSet>
#include <QTableWidget>
#include <QTextEdit>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace ClassMngr::Next::Bootstrap
{
namespace
{

constexpr quint64 MaximumPageObjectCount = 100'000;
constexpr quint64 MaximumInspectedViewCells = 65'536;
constexpr qsizetype MaximumDetailedItemModels = 256;

const char* lifecycleStateName(const Ui::PageLifecycleState state) noexcept
{
    switch (state)
    {
    case Ui::PageLifecycleState::Declared:
        return "declared";
    case Ui::PageLifecycleState::Created:
        return "created";
    case Ui::PageLifecycleState::Activated:
        return "activated";
    case Ui::PageLifecycleState::Suspended:
        return "suspended";
    case Ui::PageLifecycleState::Released:
        return "released";
    }
    return "unknown";
}

QJsonValue counterValue(const bool available, const quint64 value)
{
    return available
        ? QJsonValue(static_cast<qint64>(value))
        : QJsonValue(QJsonValue::Null);
}

QJsonObject memoryMetrics(const ProcessMemorySnapshot& snapshot)
{
    QJsonObject metrics;
    const auto insert = [&metrics](
        const QString& name, const bool available, const quint64 value) {
        metrics.insert(name, counterValue(available, value));
    };

    if (snapshot.platform == QStringLiteral("windows"))
    {
        insert(QStringLiteral("working_set_bytes"),
               snapshot.workingSetBytesAvailable, snapshot.workingSetBytes);
        insert(QStringLiteral("peak_working_set_bytes"),
               snapshot.peakWorkingSetBytesAvailable,
               snapshot.peakWorkingSetBytes);
        insert(QStringLiteral("private_usage_bytes"),
               snapshot.privateUsageBytesAvailable, snapshot.privateUsageBytes);
        insert(QStringLiteral("pagefile_usage_bytes"),
               snapshot.pagefileUsageBytesAvailable, snapshot.pagefileUsageBytes);
        insert(QStringLiteral("handle_count"), snapshot.handleCountAvailable,
               snapshot.handleCount);
        insert(QStringLiteral("thread_count"), snapshot.threadCountAvailable,
               snapshot.threadCount);
        insert(QStringLiteral("private_working_set_bytes"),
               snapshot.privateWorkingSetBytesAvailable,
               snapshot.privateWorkingSetBytes);
    }
    else if (snapshot.platform == QStringLiteral("macos"))
    {
        insert(QStringLiteral("resident_bytes"),
               snapshot.workingSetBytesAvailable, snapshot.workingSetBytes);
        insert(QStringLiteral("peak_resident_bytes"),
               snapshot.peakWorkingSetBytesAvailable,
               snapshot.peakWorkingSetBytes);
        insert(QStringLiteral("physical_footprint_bytes"),
               snapshot.privateUsageBytesAvailable, snapshot.privateUsageBytes);
        insert(QStringLiteral("compressed_bytes"),
               snapshot.pagefileUsageBytesAvailable, snapshot.pagefileUsageBytes);
        insert(QStringLiteral("thread_count"), snapshot.threadCountAvailable,
               snapshot.threadCount);
        insert(QStringLiteral("internal_bytes"),
               snapshot.privateWorkingSetBytesAvailable,
               snapshot.privateWorkingSetBytes);
    }
    else if (snapshot.platform == QStringLiteral("linux"))
    {
        insert(QStringLiteral("rss_bytes"), snapshot.workingSetBytesAvailable,
               snapshot.workingSetBytes);
        insert(QStringLiteral("peak_rss_bytes"),
               snapshot.peakWorkingSetBytesAvailable,
               snapshot.peakWorkingSetBytes);
        insert(QStringLiteral("private_usage_bytes"),
               snapshot.privateUsageBytesAvailable, snapshot.privateUsageBytes);
        insert(QStringLiteral("private_clean_and_dirty_bytes"),
               snapshot.pagefileUsageBytesAvailable, snapshot.pagefileUsageBytes);
        insert(QStringLiteral("file_descriptor_count"),
               snapshot.handleCountAvailable, snapshot.handleCount);
        insert(QStringLiteral("thread_count"), snapshot.threadCountAvailable,
               snapshot.threadCount);
        insert(QStringLiteral("anonymous_resident_bytes"),
               snapshot.privateWorkingSetBytesAvailable,
               snapshot.privateWorkingSetBytes);
        insert(QStringLiteral("private_dirty_bytes"),
               snapshot.privateDirtyBytesAvailable, snapshot.privateDirtyBytes);
    }

    return metrics;
}

QJsonValue pageCounterValue(
    const bool pageAvailable,
    const quint64 value
    )
{
    return pageAvailable
        ? QJsonValue(static_cast<qint64>(value))
        : QJsonValue(QJsonValue::Null);
}

QJsonObject pageMetrics(
    const PageLifecycleWidgetSnapshot& page,
    const QString& observation
    )
{
    QJsonValue itemModelsValue(QJsonValue::Null);
    if (page.pageInstanceAvailable)
    {
        QJsonArray itemModels;
        for (const auto& model : page.itemModels)
        {
            itemModels.append(QJsonObject{
                {QStringLiteral("object_name"), model.objectName},
                {QStringLiteral("class_name"), model.className},
                {QStringLiteral("root_rows"), pageCounterValue(true, model.rootRows)}
            });
        }
        itemModelsValue = QJsonValue(itemModels);
    }

    QJsonValue calendarRuntimeMetricsValue(QJsonValue::Null);
    if (page.calendarRuntimeMetricsAvailable)
    {
        const Ui::CalendarPageRuntimeMetrics& metrics =
            page.calendarRuntimeMetrics;
        const QString calendarObservation = observation == QStringLiteral("activated")
            ? QStringLiteral("content-ready")
            : observation == QStringLiteral("suspended")
                ? QStringLiteral("post-suspension")
                : observation;
        const Ui::CalendarEventImportRuntimeMetrics& importMetrics =
            metrics.calendarImportMetrics;
        const QJsonObject calendarImportMetrics{
            {QStringLiteral("phase"), importMetrics.phase},
            {QStringLiteral("terminal_status"), importMetrics.terminalStatus},
            {QStringLiteral("failure_message"), importMetrics.failureMessage},
            {QStringLiteral("downloaded_bytes"), importMetrics.downloadedBytes},
            {QStringLiteral("largest_transfer_chunk_bytes"),
                importMetrics.largestTransferChunkBytes},
            {QStringLiteral("transfer_chunk_count"),
                importMetrics.transferChunkCount},
            {QStringLiteral("streamed_worksheet_cell_count"),
                importMetrics.streamedWorksheetCellCount},
            {QStringLiteral("materialized_worksheet_cell_count"),
                importMetrics.materializedWorksheetCellCount},
            {QStringLiteral("parsed_event_count"),
                importMetrics.parsedEventCount},
            {QStringLiteral("typed_candidate_count_at_apply"),
                importMetrics.typedCandidateCountAtApply},
            {QStringLiteral("legacy_event_count_at_apply"),
                importMetrics.legacyEventCountAtApply},
            {QStringLiteral("imported_event_count"),
                importMetrics.importedEventCount},
            {QStringLiteral("skipped_event_count"),
                importMetrics.skippedEventCount},
            {QStringLiteral("candidate_rows_sha256"),
                importMetrics.candidateRowsSha256},
            {QStringLiteral("used_device_parser"),
                importMetrics.usedDeviceParser},
            {QStringLiteral("staged_workbook_released_before_apply"),
                importMetrics.stagedWorkbookReleasedBeforeApply},
            {QStringLiteral("staged_workbook_removed_on_cancel"),
                importMetrics.stagedWorkbookRemovedOnCancel},
            {QStringLiteral("staged_workbook_removed_at_terminal"),
                importMetrics.stagedWorkbookRemovedAtTerminal}
        };
        calendarRuntimeMetricsValue = QJsonObject{
            {QStringLiteral("observation"), calendarObservation},
            {QStringLiteral("cache_event_count"), metrics.cacheEventCount},
            {QStringLiteral("cache_date_bucket_count"), metrics.cacheDateBucketCount},
            {QStringLiteral("cache_loaded_range_count"), metrics.cacheLoadedRangeCount},
            {QStringLiteral("cache_retained_range_count"), metrics.cacheRetainedRangeCount},
            {QStringLiteral("visible_grid_day_count"), metrics.visibleGridDayCount},
            {QStringLiteral("upcoming_range_day_count"), metrics.upcomingRangeDayCount},
            {QStringLiteral("upcoming_result_count"), metrics.upcomingResultCount},
            {QStringLiteral("upcoming_result_limit"), metrics.upcomingResultLimit},
            {QStringLiteral("calendar_view_count"), metrics.calendarViewCount},
            {QStringLiteral("calendar_root_object_count"), metrics.calendarRootObjectCount},
            {QStringLiteral("calendar_view_construction_count"), metrics.calendarViewConstructionCount},
            {QStringLiteral("live_heavy_session_count"), metrics.liveHeavySessionCount},
            {QStringLiteral("peak_live_heavy_session_count"), metrics.peakLiveHeavySessionCount},
            {QStringLiteral("calendar_import_cache_refresh_count"),
                metrics.calendarImportCacheRefreshCount},
            {QStringLiteral("calendar_import"), calendarImportMetrics},
            {QStringLiteral("cache_loading"), metrics.cacheLoading}
        };
    }

    QJsonValue subPrepRuntimeMetricsValue(QJsonValue::Null);
    if (page.subPrepRuntimeMetricsAvailable)
    {
        const Ui::SubPrepPageVisualMetrics& metrics =
            page.subPrepRuntimeMetrics;
        subPrepRuntimeMetricsValue = QJsonObject{
            {QStringLiteral("observation"), observation},
            {QStringLiteral("summary_model_count"), metrics.summaryModelCount},
            {QStringLiteral("summary_class_count"), metrics.summaryClassCount},
            {QStringLiteral("teacher_summary_count"), metrics.teacherSummaryCount},
            {QStringLiteral("active_grade_row_count"), metrics.summaryRowCount},
            {QStringLiteral("selected_detail_panel_count"), metrics.selectedDetailPanelCount},
            {QStringLiteral("selected_detail_note_editor_count"), metrics.selectedDetailNoteEditorCount},
            {QStringLiteral("preparation_editor_count"), metrics.preparationEditorCount},
            {QStringLiteral("text_editor_count"), metrics.textEditorCount},
            {QStringLiteral("summary_read_count"), metrics.summaryReadCount},
            {QStringLiteral("selected_detail_read_count"), metrics.selectedDetailReadCount},
            {QStringLiteral("schedule_read_count"), metrics.scheduleReadCount},
            {QStringLiteral("visible_summary_count"), metrics.visibleSummaryCount},
            {QStringLiteral("selected_class_id"), metrics.selectedClassId},
            {QStringLiteral("selected_details_retained"), metrics.selectedDetailsRetained}
        };
    }

    return {
        {QStringLiteral("widget_count"), pageCounterValue(
            page.pageInstanceAvailable, static_cast<quint64>(page.widgetCount))},
        {QStringLiteral("widget_scan_truncated"), page.pageInstanceAvailable
            ? QJsonValue(page.widgetScanTruncated)
            : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("item_model_count"), pageCounterValue(
            page.pageInstanceAvailable,
            static_cast<quint64>(page.itemModelCount))},
        {QStringLiteral("item_model_root_rows"), pageCounterValue(
            page.pageInstanceAvailable, page.itemModelRootRows)},
        {QStringLiteral("item_models"), itemModelsValue},
        {QStringLiteral("item_models_truncated"), page.pageInstanceAvailable
            ? QJsonValue(page.itemModelsTruncated)
            : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("table_widget_count"), pageCounterValue(
            page.pageInstanceAvailable,
            static_cast<quint64>(page.tableWidgetCount))},
        {QStringLiteral("table_item_count"), pageCounterValue(
            page.pageInstanceAvailable, page.tableItemCount)},
        {QStringLiteral("inspected_table_cells"), pageCounterValue(
            page.pageInstanceAvailable, page.inspectedTableCells)},
        {QStringLiteral("table_scan_truncated"), page.pageInstanceAvailable
            ? QJsonValue(page.tableScanTruncated)
            : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("view_index_widget_count"), pageCounterValue(
            page.pageInstanceAvailable,
            static_cast<quint64>(page.viewIndexWidgetCount))},
        {QStringLiteral("view_index_widget_scan_truncated"),
            page.pageInstanceAvailable
                ? QJsonValue(page.viewIndexWidgetScanTruncated)
                : QJsonValue(QJsonValue::Null)},
        {QStringLiteral("editor_control_widget_count"), pageCounterValue(
            page.pageInstanceAvailable,
            static_cast<quint64>(page.editorControlWidgetCount))},
        {QStringLiteral("calendar_runtime_metrics"), calendarRuntimeMetricsValue},
        {QStringLiteral("sub_prep_runtime_metrics"), subPrepRuntimeMetricsValue}
    };
}

bool isEditorControl(const QWidget* widget)
{
    return qobject_cast<const QLineEdit*>(widget)
        || qobject_cast<const QTextEdit*>(widget)
        || qobject_cast<const QPlainTextEdit*>(widget)
        || qobject_cast<const QAbstractSpinBox*>(widget)
        || qobject_cast<const QDateTimeEdit*>(widget)
        || qobject_cast<const QComboBox*>(widget);
}

void writeDefaultRecord(const QByteArray& record)
{
    qInfo().noquote().nospace()
        << "ClassMngrNext page lifecycle checkpoint=" << record;
}

}  // namespace

QJsonObject pageLifecycleDiagnosticObject(
    const QString& pageKey,
    const Ui::PageLifecycleState state,
    const ProcessMemorySnapshot& memory,
    const PageLifecycleWidgetSnapshot& page
    )
{
    return {
        {QStringLiteral("event"), QStringLiteral("page-lifecycle")},
        {QStringLiteral("page_key"), pageKey},
        {QStringLiteral("state"), QString::fromLatin1(lifecycleStateName(state))},
        {QStringLiteral("captured_at"), memory.capturedAt.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("platform"), memory.platform},
        {QStringLiteral("sample_available"), memory.isAvailable},
        {QStringLiteral("memory"), memoryMetrics(memory)},
        {QStringLiteral("page_instance_available"), page.pageInstanceAvailable},
        {QStringLiteral("page"), pageMetrics(
            page, QString::fromLatin1(lifecycleStateName(state)))}
    };
}

QByteArray pageLifecycleDiagnosticRecord(
    const QString& pageKey,
    const Ui::PageLifecycleState state,
    const ProcessMemorySnapshot& memory,
    const PageLifecycleWidgetSnapshot& page
    )
{
    return QJsonDocument(pageLifecycleDiagnosticObject(
        pageKey, state, memory, page)).toJson(QJsonDocument::Compact);
}

QJsonObject pageActionDiagnosticObject(
    const QString& pageKey,
    const QString& checkpoint,
    const QString& outcome,
    const QJsonObject& details,
    const ProcessMemorySnapshot& memory,
    const PageLifecycleWidgetSnapshot& page
    )
{
    return {
        {QStringLiteral("event"), QStringLiteral("page-action-checkpoint")},
        {QStringLiteral("page_key"), pageKey},
        {QStringLiteral("checkpoint"), checkpoint},
        {QStringLiteral("outcome"), outcome},
        {QStringLiteral("details"), details},
        {QStringLiteral("captured_at"), memory.capturedAt.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("platform"), memory.platform},
        {QStringLiteral("sample_available"), memory.isAvailable},
        {QStringLiteral("memory"), memoryMetrics(memory)},
        {QStringLiteral("page_instance_available"), page.pageInstanceAvailable},
        {QStringLiteral("page"), pageMetrics(page, checkpoint)}
    };
}

QByteArray pageActionDiagnosticRecord(
    const QString& pageKey,
    const QString& checkpoint,
    const QString& outcome,
    const QJsonObject& details,
    const ProcessMemorySnapshot& memory,
    const PageLifecycleWidgetSnapshot& page
    )
{
    return QJsonDocument(pageActionDiagnosticObject(
        pageKey, checkpoint, outcome, details, memory, page))
        .toJson(QJsonDocument::Compact);
}

QJsonObject pageInstanceDestroyedDiagnosticObject(
    const QString& pageKey,
    const ProcessMemorySnapshot& memory
    )
{
    const PageLifecycleWidgetSnapshot pageUnavailable;
    return {
        {QStringLiteral("event"), QStringLiteral("page-instance-destroyed")},
        {QStringLiteral("page_key"), pageKey},
        {QStringLiteral("captured_at"), memory.capturedAt.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("platform"), memory.platform},
        {QStringLiteral("sample_available"), memory.isAvailable},
        {QStringLiteral("memory"), memoryMetrics(memory)},
        {QStringLiteral("page_instance_available"), false},
        {QStringLiteral("page"), pageMetrics(
            pageUnavailable, QStringLiteral("released"))}
    };
}

QByteArray pageInstanceDestroyedDiagnosticRecord(
    const QString& pageKey,
    const ProcessMemorySnapshot& memory
    )
{
    return QJsonDocument(pageInstanceDestroyedDiagnosticObject(
        pageKey, memory)).toJson(QJsonDocument::Compact);
}

PageLifecycleWidgetSnapshot pageLifecycleWidgetSnapshot(const QWidget* page)
{
    PageLifecycleWidgetSnapshot snapshot;
    if (!page)
    {
        return snapshot;
    }

    snapshot.pageInstanceAvailable = true;
    QVector<const QObject*> pending;
    pending.reserve(128);
    pending.push_back(page);
    QVector<const QAbstractItemView*> views;
    views.reserve(8);
    QSet<const QAbstractItemModel*> models;

    quint64 visitedObjects = 0;
    while (!pending.isEmpty() && visitedObjects < MaximumPageObjectCount)
    {
        const QObject* const current = pending.takeLast();
        ++visitedObjects;
        if (const auto* const widget = qobject_cast<const QWidget*>(current))
        {
            ++snapshot.widgetCount;
            if (isEditorControl(widget))
            {
                ++snapshot.editorControlWidgetCount;
            }
            if (const auto* const view = qobject_cast<const QAbstractItemView*>(widget))
            {
                views.push_back(view);
                if (const QAbstractItemModel* const model = view->model())
                {
                    if (!models.contains(model))
                    {
                        models.insert(model);
                        ++snapshot.itemModelCount;
                        const quint64 rootRows = static_cast<quint64>(
                            std::max(0, model->rowCount()));
                        snapshot.itemModelRootRows += rootRows;
                        if (snapshot.itemModels.size()
                            < MaximumDetailedItemModels)
                        {
                            snapshot.itemModels.push_back({
                                .objectName = model->objectName(),
                                .className = QString::fromLatin1(
                                    model->metaObject()->className()),
                                .rootRows = rootRows
                            });
                        }
                        else
                        {
                            snapshot.itemModelsTruncated = true;
                        }
                    }
                }
            }
            if (qobject_cast<const QTableWidget*>(widget))
            {
                ++snapshot.tableWidgetCount;
            }
        }

        const QObjectList& children = current->children();
        for (const QObject* const child : children)
        {
            if (!child)
            {
                continue;
            }
            if (pending.size() >= static_cast<qsizetype>(MaximumPageObjectCount))
            {
                snapshot.widgetScanTruncated = true;
                break;
            }
            if (pending.size() < static_cast<qsizetype>(MaximumPageObjectCount))
            {
                pending.push_back(child);
            }
        }
    }
    snapshot.widgetScanTruncated = snapshot.widgetScanTruncated
        || !pending.isEmpty() || visitedObjects >= MaximumPageObjectCount;

    quint64 inspectedCells = 0;
    for (const QAbstractItemView* const view : views)
    {
        const QAbstractItemModel* const model = view->model();
        if (!model)
        {
            continue;
        }
        const auto* const table = qobject_cast<const QTableWidget*>(view);
        const int modelRows = model->rowCount();
        const int modelColumns = model->columnCount();
        const quint64 modelCells = static_cast<quint64>(std::max(0, modelRows))
            * static_cast<quint64>(std::max(0, modelColumns));
        const quint64 remaining = MaximumInspectedViewCells - inspectedCells;
        const quint64 cellsToInspect = std::min(modelCells, remaining);
        if (cellsToInspect < modelCells)
        {
            snapshot.viewIndexWidgetScanTruncated = true;
            if (table)
            {
                snapshot.tableScanTruncated = true;
            }
        }

        quint64 visitedForView = 0;
        for (int row = 0;
             row < modelRows && visitedForView < cellsToInspect;
             ++row)
        {
            for (int column = 0;
                 column < modelColumns && visitedForView < cellsToInspect;
                 ++column)
            {
                const QModelIndex index = model->index(row, column);
                if (view->indexWidget(index))
                {
                    ++snapshot.viewIndexWidgetCount;
                }
                if (table)
                {
                    ++snapshot.inspectedTableCells;
                    if (table->item(row, column))
                    {
                        ++snapshot.tableItemCount;
                    }
                }
                ++visitedForView;
                ++inspectedCells;
            }
        }
    }

    return snapshot;
}

QtPageLifecycleDiagnosticsRecorder::QtPageLifecycleDiagnosticsRecorder(
    Ui::PageHost& pageHost,
    std::unique_ptr<ProcessMemorySnapshotProvider> snapshotProvider,
    RecordWriter recordWriter,
    CalendarRuntimeMetricsReader calendarRuntimeMetricsReader,
    SubPrepRuntimeMetricsReader subPrepRuntimeMetricsReader
    )
    : QObject(nullptr)
    , m_pageHost(pageHost)
    , m_snapshotProvider(snapshotProvider
          ? std::move(snapshotProvider)
          : std::make_unique<PlatformProcessMemorySnapshotProvider>())
    , m_recordWriter(recordWriter
          ? std::move(recordWriter)
          : RecordWriter(writeDefaultRecord))
    , m_calendarRuntimeMetricsReader(
          std::move(calendarRuntimeMetricsReader))
    , m_subPrepRuntimeMetricsReader(
          std::move(subPrepRuntimeMetricsReader))
{
    m_connection = QObject::connect(
        &m_pageHost,
        &Ui::PageHost::pageLifecycleStateChanged,
        this,
        [this](const QString& pageKey, const Ui::PageLifecycleState state) {
            record(pageKey, state);
        }
        );
}

void QtPageLifecycleDiagnosticsRecorder::recordPageActionCheckpoint(
    const QString& pageKey,
    const QString& checkpoint,
    const QString& outcome,
    const QJsonObject& details
    ) noexcept
{
    try
    {
        const ProcessMemorySnapshot memory = m_snapshotProvider->snapshot();
        const Ui::Page* const page = m_pageHost.pageInstance(pageKey);
        PageLifecycleWidgetSnapshot widgets = pageLifecycleWidgetSnapshot(page);
        if (page && m_calendarRuntimeMetricsReader)
        {
            if (const auto metrics = m_calendarRuntimeMetricsReader(page))
            {
                widgets.calendarRuntimeMetricsAvailable = true;
                widgets.calendarRuntimeMetrics = *metrics;
            }
        }
        if (page && m_subPrepRuntimeMetricsReader)
        {
            if (const auto metrics = m_subPrepRuntimeMetricsReader(page))
            {
                widgets.subPrepRuntimeMetricsAvailable = true;
                widgets.subPrepRuntimeMetrics = *metrics;
            }
        }
        m_recordWriter(pageActionDiagnosticRecord(
            pageKey, checkpoint, outcome, details, memory, widgets));
    }
    catch (...)
    {
        qWarning("ClassMngrNext page action diagnostics failed.");
    }
}

QtPageLifecycleDiagnosticsRecorder::~QtPageLifecycleDiagnosticsRecorder()
{
    QObject::disconnect(m_connection);
    for (const QMetaObject::Connection& connection : m_destroyConnections)
    {
        QObject::disconnect(connection);
    }
    m_destroyConnections.clear();
}

void QtPageLifecycleDiagnosticsRecorder::record(
    const QString& pageKey,
    const Ui::PageLifecycleState state
    ) noexcept
{
    try
    {
        if (state == Ui::PageLifecycleState::Activated)
        {
            observeActivatedPage(pageKey);
        }
        const ProcessMemorySnapshot memory = m_snapshotProvider->snapshot();
        const Ui::Page* const page = m_pageHost.pageInstance(pageKey);
        PageLifecycleWidgetSnapshot widgets = pageLifecycleWidgetSnapshot(page);
        if (page && m_calendarRuntimeMetricsReader)
        {
            if (const auto metrics = m_calendarRuntimeMetricsReader(page))
            {
                widgets.calendarRuntimeMetricsAvailable = true;
                widgets.calendarRuntimeMetrics = *metrics;
            }
        }
        if (page && m_subPrepRuntimeMetricsReader)
        {
            if (const auto metrics = m_subPrepRuntimeMetricsReader(page))
            {
                widgets.subPrepRuntimeMetricsAvailable = true;
                widgets.subPrepRuntimeMetrics = *metrics;
            }
        }
        m_recordWriter(pageLifecycleDiagnosticRecord(
            pageKey, state, memory, widgets));
    }
    catch (...)
    {
        qWarning("ClassMngrNext page lifecycle diagnostics failed.");
    }
}

void QtPageLifecycleDiagnosticsRecorder::observeActivatedPage(
    const QString& pageKey
    )
{
    Ui::Page* const page = m_pageHost.pageInstance(pageKey);
    if (!page)
    {
        return;
    }

    // Use the pointer only as an identity key. The destroyed callback does
    // not dereference it and removes it as soon as QObject reports teardown.
    // Release/destruction removes entries, so reactivation does not grow this
    // map across the PageHost-owned instances over time.
    const QObject* const identity = page;
    if (m_destroyConnections.contains(identity))
    {
        return;
    }
    const QMetaObject::Connection connection = QObject::connect(
        page,
        &QObject::destroyed,
        this,
        [this, pageKey](QObject* const destroyedObject) {
            const QObject* const destroyedIdentity = destroyedObject;
            m_destroyConnections.remove(destroyedIdentity);
            recordDestroyed(pageKey);
        }
        );
    if (connection)
    {
        m_destroyConnections.insert(identity, connection);
    }
}

void QtPageLifecycleDiagnosticsRecorder::recordDestroyed(
    const QString& pageKey
    ) noexcept
{
    try
    {
        const ProcessMemorySnapshot memory = m_snapshotProvider->snapshot();
        m_recordWriter(pageInstanceDestroyedDiagnosticRecord(pageKey, memory));
    }
    catch (...)
    {
        qWarning("ClassMngrNext page instance destruction diagnostics failed.");
    }
}

}  // namespace ClassMngr::Next::Bootstrap

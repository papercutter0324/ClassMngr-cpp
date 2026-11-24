#pragma once

#include "core/process_memory_snapshot.h"
#include "next/application/roster_save_use_case.h"
#include "next/ui/calendar_page.h"
#include "next/ui/page_host.h"
#include "next/ui/sub_prep_page.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QMetaObject>
#include <QVector>

#include <QObject>

#include <functional>
#include <memory>
#include <optional>

namespace ClassMngr::Next::Bootstrap
{

struct PageLifecycleWidgetSnapshot final
{
    struct ItemModel final
    {
        QString objectName;
        QString className;
        quint64 rootRows = 0;
    };

    bool pageInstanceAvailable = false;
    int widgetCount = 0;
    bool widgetScanTruncated = false;
    int itemModelCount = 0;
    quint64 itemModelRootRows = 0;
    bool itemModelsTruncated = false;
    QVector<ItemModel> itemModels;
    int tableWidgetCount = 0;
    quint64 tableItemCount = 0;
    quint64 inspectedTableCells = 0;
    bool tableScanTruncated = false;
    int viewIndexWidgetCount = 0;
    bool viewIndexWidgetScanTruncated = false;
    int editorControlWidgetCount = 0;
    bool calendarRuntimeMetricsAvailable = false;
    Ui::CalendarPageRuntimeMetrics calendarRuntimeMetrics;
    bool subPrepRuntimeMetricsAvailable = false;
    Ui::SubPrepPageVisualMetrics subPrepRuntimeMetrics;
};

// Produces one compact, deterministic JSON record for each PageHost state
// transition. Missing per-counter values are represented as JSON null.
[[nodiscard]] QJsonObject pageLifecycleDiagnosticObject(
    const QString& pageKey,
    Ui::PageLifecycleState state,
    const ProcessMemorySnapshot& memory,
    const PageLifecycleWidgetSnapshot& page
    );
[[nodiscard]] QByteArray pageLifecycleDiagnosticRecord(
    const QString& pageKey,
    Ui::PageLifecycleState state,
    const ProcessMemorySnapshot& memory,
    const PageLifecycleWidgetSnapshot& page
    );
[[nodiscard]] QJsonObject pageActionDiagnosticObject(
    const QString& pageKey,
    const QString& checkpoint,
    const QString& outcome,
    const QJsonObject& details,
    const ProcessMemorySnapshot& memory,
    const PageLifecycleWidgetSnapshot& page
    );
[[nodiscard]] QByteArray pageActionDiagnosticRecord(
    const QString& pageKey,
    const QString& checkpoint,
    const QString& outcome,
    const QJsonObject& details,
    const ProcessMemorySnapshot& memory,
    const PageLifecycleWidgetSnapshot& page
    );
[[nodiscard]] QJsonObject pageInstanceDestroyedDiagnosticObject(
    const QString& pageKey,
    const ProcessMemorySnapshot& memory
    );
[[nodiscard]] QByteArray pageInstanceDestroyedDiagnosticRecord(
    const QString& pageKey,
    const ProcessMemorySnapshot& memory
    );
[[nodiscard]] QJsonObject defaultProcessHeapSnapshotObject(
    const DefaultProcessHeapSnapshot& snapshot
    );
[[nodiscard]] QByteArray defaultProcessHeapRosterCheckpointRecord(
    const QString& checkpoint,
    const QString& outcome,
    quint64 eventSequence,
    std::optional<quint64> rosterWidgetInstanceId,
    std::optional<int> row,
    std::optional<int> column,
    const DefaultProcessHeapSnapshot& snapshot
    );
[[nodiscard]] QByteArray rosterSaveStageDiagnosticRecord(
    Application::RosterSaveDiagnosticStage stage,
    Application::RosterSaveDiagnosticOutcome outcome,
    quint64 eventSequence,
    quint64 rosterWidgetInstanceId,
    quint64 saveSequence,
    const ProcessMemorySnapshot& memory,
    const DefaultProcessHeapSnapshot& heap
    );

[[nodiscard]] PageLifecycleWidgetSnapshot pageLifecycleWidgetSnapshot(
    const QWidget* page
    );

// Connects to PageHost's existing lifecycle event and writes process-wide
// memory plus bounded widget/model observations. The optional writer and
// provider make the record path directly testable without changing PageHost.
class QtPageLifecycleDiagnosticsRecorder final : public QObject
{
public:
    using RecordWriter = std::function<void(const QByteArray&)>;
    using CalendarRuntimeMetricsReader = std::function<
        std::optional<Ui::CalendarPageRuntimeMetrics>(const QWidget*)>;
    using SubPrepRuntimeMetricsReader = std::function<
        std::optional<Ui::SubPrepPageVisualMetrics>(const QWidget*)>;
    using DefaultProcessHeapSnapshotReader =
        std::function<DefaultProcessHeapSnapshot()>;

    explicit QtPageLifecycleDiagnosticsRecorder(
        Ui::PageHost& pageHost,
        std::unique_ptr<ProcessMemorySnapshotProvider> snapshotProvider = {},
        RecordWriter recordWriter = {},
        CalendarRuntimeMetricsReader calendarRuntimeMetricsReader = {},
        SubPrepRuntimeMetricsReader subPrepRuntimeMetricsReader = {},
        bool defaultProcessHeapDiagnosticsEnabled = false,
        DefaultProcessHeapSnapshotReader defaultProcessHeapSnapshotReader = {}
        );
    void recordPageActionCheckpoint(
        const QString& pageKey,
        const QString& checkpoint,
        const QString& outcome,
        const QJsonObject& details = {}
        ) noexcept;
    void recordRosterDefaultHeapCheckpoint(
        quint64 rosterWidgetInstanceId,
        const QString& checkpoint,
        const QString& outcome,
        std::optional<int> row = std::nullopt,
        std::optional<int> column = std::nullopt
        ) noexcept;
    void recordRosterSaveStageCheckpoint(
        quint64 rosterWidgetInstanceId,
        quint64 saveSequence,
        Application::RosterSaveDiagnosticStage stage,
        Application::RosterSaveDiagnosticOutcome outcome
        ) noexcept;
    [[nodiscard]] bool defaultProcessHeapDiagnosticsEnabled() const noexcept;
    ~QtPageLifecycleDiagnosticsRecorder() override;

    QtPageLifecycleDiagnosticsRecorder(
        const QtPageLifecycleDiagnosticsRecorder&
        ) = delete;
    QtPageLifecycleDiagnosticsRecorder& operator=(
        const QtPageLifecycleDiagnosticsRecorder&
        ) = delete;

private:
    void record(const QString& pageKey, Ui::PageLifecycleState state) noexcept;
    void observeActivatedPage(const QString& pageKey);
    void recordDestroyed(const QString& pageKey) noexcept;
    void recordDefaultHeapCheckpoint(
        const QString& checkpoint,
        const QString& outcome,
        std::optional<quint64> rosterWidgetInstanceId,
        std::optional<int> row,
        std::optional<int> column
        ) noexcept;

    Ui::PageHost& m_pageHost;
    std::unique_ptr<ProcessMemorySnapshotProvider> m_snapshotProvider;
    RecordWriter m_recordWriter;
    CalendarRuntimeMetricsReader m_calendarRuntimeMetricsReader;
    SubPrepRuntimeMetricsReader m_subPrepRuntimeMetricsReader;
    bool m_defaultProcessHeapDiagnosticsEnabled = false;
    bool m_defaultHeapBaselineRecorded = false;
    quint64 m_nextDefaultHeapEventSequence = 1;
    DefaultProcessHeapSnapshotReader m_defaultProcessHeapSnapshotReader;
    QMetaObject::Connection m_connection;
    QHash<const QObject*, QMetaObject::Connection> m_destroyConnections;
};

}  // namespace ClassMngr::Next::Bootstrap

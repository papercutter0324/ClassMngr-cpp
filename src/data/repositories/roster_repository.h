#pragma once

#include "repository_error.h"
#include "domain/models/roster.h"

#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QStringList>

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>

inline constexpr std::size_t kRosterRepositoryOutputMaxColumns = 128;
inline constexpr std::size_t kRosterRepositoryOutputMaxRows = 4'096;
inline constexpr std::size_t kRosterRepositoryOutputMaxCells = 1'000'000;
inline constexpr std::size_t kRosterRepositoryOutputMaxCellBytes = 16'384;
inline constexpr std::size_t kRosterRepositoryOutputMaxColumnNameBytes = 256;
inline constexpr std::size_t kRosterRepositoryOutputMaxTextBytes =
    32 * 1024 * 1024;

struct RosterReadOutcome final
{
    Roster roster;
    bool parentMissing = false;
};

struct RosterWindowReadOutcome final
{
    Roster roster;
    bool parentMissing = false;
    bool hasRowsOutsideWindow = false;
};

class RosterRepository
{
public:
    // Replaces [0, 25), preserving sparse tail rows at absolute indices.
    // Only explicitly confirmed, displayed custom columns may lose tail cells.
    struct WindowSaveRequest final
    {
        int classId = 0;
        Roster roster;
        QStringList removedCustomColumns;
    };

    [[nodiscard]] RepositoryStatus saveRosterWindowsWithParentCheck(
        const QList<WindowSaveRequest>& windows);


    struct ColumnNamesForClass final
    {
        int classId = 0;
        QStringList columns;
    };

    struct ParentAwareColumnNamesForClass final
    {
        int classId = 0;
        QStringList columns;
        bool parentMissing = false;
    };

    struct FullRosterReadRecord final
    {
        int classId = 0;
        Roster roster;
        bool parentMissing = false;
    };

    using TemplatePrintReadRecord = FullRosterReadRecord;

    struct TemplatePrintBatchReadMetrics final
    {
        int callCount = 0;
        int requestedClassCount = 0;
        int columnStatementCount = 0;
        int cellStatementCount = 0;
    };

    struct MyClassesStudentCountReadEntry final
    {
        int classId = 0;
        RepositoryResult<int> studentCount;
        bool parentMissing = false;
    };

    struct MyClassesStudentCountBatchReadMetrics final
    {
        int callCount = 0;
        int requestedClassCount = 0;
        int columnStatementCount = 0;
        int dataStatementCount = 0;
        int fallbackClassReadCount = 0;
    };

    struct SubPrepStudentCountReadEntry final
    {
        int classId = 0;
        RepositoryResult<int> studentCount = 0;
        int columnCount = 0;
        int rowCount = 0;
        int cellCount = 0;
    };

    struct SpeakingEvaluationRosterNamePairReadRecord final
    {
        QString englishName;
        QString koreanName;
    };

    struct SpeakingEvaluationRosterNamesReadRecord final
    {
        bool hasEnglishColumn = false;
        bool hasKoreanColumn = false;
        QList<SpeakingEvaluationRosterNamePairReadRecord> rows;
    };

    struct SpeakingEvaluationRosterNamesReadOutcome final
    {
        SpeakingEvaluationRosterNamesReadRecord rosterNames;
        bool parentMissing = false;
    };

    struct ClassAnalyticsRosterNamesMetadata final
    {
        std::size_t rowCount = 0;
        bool hasEnglishColumn = false;
        bool hasKoreanColumn = false;
    };

    struct ClassAnalyticsRosterNameCell final
    {
        std::size_t rowIndex = 0;
        bool isEnglishName = false;
        QString value;
    };

    struct SubPrepStudentCountBatchReadMetrics final
    {
        int callCount = 0;
        int requestedClassCount = 0;
        int columnStatementCount = 0;
        int dataStatementCount = 0;
        int fallbackClassReadCount = 0;
    };

    explicit RosterRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryStatus saveRoster(
        int classId,
        const Roster& roster
        );

    // Used by typed callers that need to distinguish a missing parent class
    // from other repository failures.
    [[nodiscard]] RepositoryStatus saveRosterWithParentCheck(
        int classId,
        const Roster& roster
        );

    [[nodiscard]] RepositoryStatus saveRosters(
        const QList<QPair<int, Roster>>& rosters
        );

    [[nodiscard]] RepositoryResult<Roster> loadRoster(
        int classId
        );
    // Adds parent provenance while retaining requested-ID child row loading.
    [[nodiscard]] RepositoryResult<RosterReadOutcome> loadRosterDetailed(
        int classId
        );
    // Loads every ordered column and width, but materializes cells only for
    // rows [0, rowLimit). Reports any persisted row outside that window.
    [[nodiscard]] RepositoryResult<RosterWindowReadOutcome> loadRosterWindowDetailed(
        int classId,
        int rowLimit
        );

    // Reads only the first English/Korean name columns and preserves the
    // legacy materialized-row shape, including sparse rows caused by any
    // valid roster cell.
    [[nodiscard]] RepositoryResult<SpeakingEvaluationRosterNamesReadRecord>
    loadSpeakingEvaluationRosterNames(int classId);
    // Adds parent provenance for typed callers while keeping the legacy
    // projection API and its requested-ID child-row behavior intact.
    [[nodiscard]] RepositoryResult<SpeakingEvaluationRosterNamesReadOutcome>
    loadSpeakingEvaluationRosterNamesDetailed(int classId);

    // Reports sparse row sizing once, then streams only the first exact
    // English/Korean columns. Unrelated in-range columns still affect sizing.
    // Consumer exceptions propagate to the caller.
    [[nodiscard]] RepositoryStatus forEachClassAnalyticsRosterNameCell(
        int classId,
        const std::function<void(const ClassAnalyticsRosterNamesMetadata&)>&
            metadataConsumer,
        const std::function<void(const ClassAnalyticsRosterNameCell&)>&
            cellConsumer
        );

    [[nodiscard]] RepositoryResult<QList<TemplatePrintReadRecord>>
    loadRostersForTemplatePrint(const QList<int>& classIds);
    // Loads complete roster snapshots in request order, including widths and
    // sparse row shape. Uses one column read and, when needed, one cell read.
    [[nodiscard]] RepositoryResult<QList<FullRosterReadRecord>>
    loadFullRosters(const QList<int>& classIds);
    [[nodiscard]] const TemplatePrintBatchReadMetrics&
    templatePrintBatchReadMetrics() const noexcept;

    // Reads ordered column names for every requested class in one statement.
    // Classes without roster columns are returned with an empty column list.
    [[nodiscard]] RepositoryResult<QList<ColumnNamesForClass>>
    loadRosterColumnNamesForClasses(
        const QList<int>& classIds
        );
    // Reads columns through the requested parent class. Confirmed missing
    // parents are retained as entries with parentMissing set; SQL failures
    // remain failures of the outer batch result.
    [[nodiscard]] RepositoryResult<QList<ParentAwareColumnNamesForClass>>
    loadRosterColumnNamesForClassesWithParentProvenance(
        const QList<int>& classIds
        );

    // Counts nonblank English/Korean cells without constructing roster rows.
    // A successful batch uses one ordered column read and one sparse cell read.
    [[nodiscard]] RepositoryResult<QList<MyClassesStudentCountReadEntry>>
    loadMyClassesStudentCountRecords(
        const QList<int>& classIds
        );
    [[nodiscard]] const MyClassesStudentCountBatchReadMetrics&
    myClassesStudentCountBatchReadMetrics() const noexcept;

    // Reads Sub Prep print counts and legacy profiler dimensions in set based
    // statements. Failures stay local to each class in the returned entries.
    [[nodiscard]] RepositoryResult<QList<SubPrepStudentCountReadEntry>>
    loadSubPrepStudentCountRecords(
        const QList<int>& classIds
        );
    [[nodiscard]] const SubPrepStudentCountBatchReadMetrics&
    subPrepStudentCountBatchReadMetrics() const noexcept;

    // Streams sparse cells for the requested classes without materializing
    // roster snapshots. Rows outside [0, rowLimit) are omitted.
    [[nodiscard]] RepositoryStatus forEachRosterDataCellForClasses(
        const QList<int>& classIds,
        int rowLimit,
        const std::function<void(int, int, int, const QString&)>& consumer
        );
    // Streams sparse cells only through an existing parent class. Orphaned
    // roster rows are excluded even when their class id was requested.
    [[nodiscard]] RepositoryStatus forEachRosterDataCellForExistingClasses(
        const QList<int>& classIds,
        int rowLimit,
        const std::function<void(int, int, int, const QString&)>& consumer
        );

    // Loads only the requested columns and rejects row, cell, or UTF-8 byte
    // limits before allocating the projected cell matrix.
    [[nodiscard]] RepositoryResult<Roster> loadRosterForOutput(
        int classId,
        const QStringList& requestedColumns,
        std::size_t maxRows,
        std::size_t maxCells,
        std::size_t maxTextBytes
        );

    [[nodiscard]] RepositoryResult<int> getRosterStudentCount(
        int classId
        );

private:
    [[nodiscard]] RepositoryStatus writeRosterWindow(const WindowSaveRequest& window);
    [[nodiscard]] RepositoryStatus writeRoster(
        int classId,
        const Roster& roster
        );
    [[nodiscard]] RepositoryResult<MyClassesStudentCountReadEntry>
    loadMyClassesStudentCountRecord(int classId);
    [[nodiscard]] RepositoryResult<SpeakingEvaluationRosterNamesReadOutcome>
    loadSpeakingEvaluationRosterNamesImpl(int classId, bool parentRooted);
    [[nodiscard]] RepositoryResult<SubPrepStudentCountReadEntry>
    loadSubPrepStudentCountRecord(int classId);
    [[nodiscard]] RepositoryResult<SubPrepStudentCountReadEntry>
    loadSingleClassSubPrepStudentCountRecord(int classId);
    [[nodiscard]] RepositoryResult<RosterWindowReadOutcome> loadRosterDetailedImpl(
        int classId,
        std::optional<int> rowLimit
        );
    [[nodiscard]] RepositoryResult<QList<FullRosterReadRecord>> loadFullRostersImpl(
        const QList<int>& classIds,
        TemplatePrintBatchReadMetrics* templatePrintMetrics,
        bool templatePrintContext
        );

    QSqlDatabase& m_database;
    MyClassesStudentCountBatchReadMetrics
        m_myClassesStudentCountBatchReadMetrics;
    SubPrepStudentCountBatchReadMetrics
        m_subPrepStudentCountBatchReadMetrics;
    TemplatePrintBatchReadMetrics m_templatePrintBatchReadMetrics;
};

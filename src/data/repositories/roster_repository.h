#pragma once

#include "core/result.h"
#include "domain/models/roster.h"

#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QStringList>

#include <cstddef>
#include <expected>
#include <functional>

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

class RosterRepository
{
public:
    struct SaveFailure final
    {
        enum class Kind
        {
            ParentClassMissing,
            Technical
        };

        Kind kind = Kind::Technical;
        QString diagnostic;
    };

    using SaveResult = std::expected<void, SaveFailure>;

    struct ColumnNamesForClass final
    {
        int classId = 0;
        QStringList columns;
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
        Result<int> studentCount;
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
        Result<int> studentCount = 0;
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

    [[nodiscard]] Status saveRoster(
        int classId,
        const Roster& roster
        );

    // Used by typed callers that need to distinguish a missing parent class
    // from other repository failures.
    [[nodiscard]] SaveResult saveRosterWithParentCheck(
        int classId,
        const Roster& roster
        );

    [[nodiscard]] Status saveRosters(
        const QList<QPair<int, Roster>>& rosters
        );

    [[nodiscard]] Result<Roster> loadRoster(
        int classId
        );
    // Adds parent provenance while retaining requested-ID child row loading.
    [[nodiscard]] Result<RosterReadOutcome> loadRosterDetailed(
        int classId
        );

    // Reads only the first English/Korean name columns and preserves the
    // legacy materialized-row shape, including sparse rows caused by any
    // valid roster cell.
    [[nodiscard]] Result<SpeakingEvaluationRosterNamesReadRecord>
    loadSpeakingEvaluationRosterNames(int classId);
    // Adds parent provenance for typed callers while keeping the legacy
    // projection API and its requested-ID child-row behavior intact.
    [[nodiscard]] Result<SpeakingEvaluationRosterNamesReadOutcome>
    loadSpeakingEvaluationRosterNamesDetailed(int classId);

    [[nodiscard]] Result<QList<TemplatePrintReadRecord>>
    loadRostersForTemplatePrint(const QList<int>& classIds);
    // Loads complete roster snapshots in request order, including widths and
    // sparse row shape. Uses one column read and, when needed, one cell read.
    [[nodiscard]] Result<QList<FullRosterReadRecord>>
    loadFullRosters(const QList<int>& classIds);
    [[nodiscard]] const TemplatePrintBatchReadMetrics&
    templatePrintBatchReadMetrics() const noexcept;

    // Reads ordered column names for every requested class in one statement.
    // Classes without roster columns are returned with an empty column list.
    [[nodiscard]] Result<QList<ColumnNamesForClass>>
    loadRosterColumnNamesForClasses(
        const QList<int>& classIds
        );

    // Counts nonblank English/Korean cells without constructing roster rows.
    // A successful batch uses one ordered column read and one sparse cell read.
    [[nodiscard]] Result<QList<MyClassesStudentCountReadEntry>>
    loadMyClassesStudentCountRecords(
        const QList<int>& classIds
        );
    [[nodiscard]] const MyClassesStudentCountBatchReadMetrics&
    myClassesStudentCountBatchReadMetrics() const noexcept;

    // Reads Sub Prep print counts and legacy profiler dimensions in set based
    // statements. Failures stay local to each class in the returned entries.
    [[nodiscard]] Result<QList<SubPrepStudentCountReadEntry>>
    loadSubPrepStudentCountRecords(
        const QList<int>& classIds
        );
    [[nodiscard]] const SubPrepStudentCountBatchReadMetrics&
    subPrepStudentCountBatchReadMetrics() const noexcept;

    // Streams sparse cells for the requested classes without materializing
    // roster snapshots. Rows outside [0, rowLimit) are omitted.
    [[nodiscard]] Status forEachRosterDataCellForClasses(
        const QList<int>& classIds,
        int rowLimit,
        const std::function<void(int, int, int, const QString&)>& consumer
        );

    // Loads only the requested columns and rejects row, cell, or UTF-8 byte
    // limits before allocating the projected cell matrix.
    [[nodiscard]] Result<Roster> loadRosterForOutput(
        int classId,
        const QStringList& requestedColumns,
        std::size_t maxRows,
        std::size_t maxCells,
        std::size_t maxTextBytes
        );

    [[nodiscard]] Result<int> getRosterStudentCount(
        int classId
        );

private:
    [[nodiscard]] Status writeRoster(
        int classId,
        const Roster& roster
        );
    [[nodiscard]] Result<MyClassesStudentCountReadEntry>
    loadMyClassesStudentCountRecord(int classId);
    [[nodiscard]] Result<SpeakingEvaluationRosterNamesReadOutcome>
    loadSpeakingEvaluationRosterNamesImpl(int classId, bool parentRooted);
    [[nodiscard]] Result<SubPrepStudentCountReadEntry>
    loadSubPrepStudentCountRecord(int classId);
    [[nodiscard]] Result<QList<FullRosterReadRecord>> loadFullRostersImpl(
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

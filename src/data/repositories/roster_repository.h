#pragma once

#include "core/result.h"
#include "domain/models/roster.h"

#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QStringList>

#include <cstddef>
#include <functional>

inline constexpr std::size_t kRosterRepositoryOutputMaxColumns = 128;
inline constexpr std::size_t kRosterRepositoryOutputMaxRows = 4'096;
inline constexpr std::size_t kRosterRepositoryOutputMaxCells = 1'000'000;
inline constexpr std::size_t kRosterRepositoryOutputMaxCellBytes = 16'384;
inline constexpr std::size_t kRosterRepositoryOutputMaxColumnNameBytes = 256;
inline constexpr std::size_t kRosterRepositoryOutputMaxTextBytes =
    32 * 1024 * 1024;

class RosterRepository
{
public:
    struct ColumnNamesForClass final
    {
        int classId = 0;
        QStringList columns;
    };

    struct MyClassesStudentCountReadEntry final
    {
        int classId = 0;
        Result<int> studentCount;
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

    [[nodiscard]] Status saveRosters(
        const QList<QPair<int, Roster>>& rosters
        );

    [[nodiscard]] Result<Roster> loadRoster(
        int classId
        );

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
    [[nodiscard]] Result<int> loadMyClassesStudentCountRecord(int classId);
    [[nodiscard]] Result<SubPrepStudentCountReadEntry>
    loadSubPrepStudentCountRecord(int classId);

    QSqlDatabase& m_database;
    MyClassesStudentCountBatchReadMetrics
        m_myClassesStudentCountBatchReadMetrics;
    SubPrepStudentCountBatchReadMetrics
        m_subPrepStudentCountBatchReadMetrics;
};

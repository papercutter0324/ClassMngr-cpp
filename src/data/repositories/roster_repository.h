#pragma once

#include "core/result.h"
#include "domain/models/roster.h"

#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QStringList>

#include <cstddef>

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

    QSqlDatabase& m_database;
};

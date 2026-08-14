#pragma once

#include "core/result.h"
#include "domain/models/roster.h"

#include <QList>
#include <QPair>
#include <QSqlDatabase>

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

    Roster loadRoster(
        int classId
        );

    int getRosterStudentCount(
        int classId
        );

private:
    [[nodiscard]] Status writeRoster(
        int classId,
        const Roster& roster
        );

    QSqlDatabase& m_database;
};

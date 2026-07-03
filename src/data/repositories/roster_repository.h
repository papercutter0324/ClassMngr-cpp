#pragma once

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

    void saveRoster(
        int classId,
        const Roster& roster
        );

    bool saveRosters(
        const QList<QPair<int, Roster>>& rosters
        );

    Roster loadRoster(
        int classId
        );

    int getRosterStudentCount(
        int classId
        );

private:
    bool writeRoster(
        int classId,
        const Roster& roster
        );

    QSqlDatabase& m_database;
};

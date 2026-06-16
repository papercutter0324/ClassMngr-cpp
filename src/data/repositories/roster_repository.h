#pragma once

#include "domain/models/roster.h"

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

    Roster loadRoster(
        int classId
        );

    int getRosterStudentCount(
        int classId
        );

private:
    QSqlDatabase& m_database;
};

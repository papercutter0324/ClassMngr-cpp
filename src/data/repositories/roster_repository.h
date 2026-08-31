#pragma once

#include "core/result.h"
#include "domain/models/roster.h"

#include <QList>
#include <QPair>
#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class RosterRepository
{
public:
    explicit RosterRepository(
        QSqlDatabase& database
        );
    ~RosterRepository();

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

    [[nodiscard]] Result<int> getRosterStudentCount(
        int classId
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation,
        int classId
        );

    QSqlDatabase& m_database;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};

#pragma once

#include "core/result.h"
#include "domain/models/gs_team_member.h"

#include <QList>
#include <QSqlDatabase>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class GsTeamRepository
{
public:
    explicit GsTeamRepository(QSqlDatabase& database);
    ~GsTeamRepository();

    [[nodiscard]] Result<QList<GsTeamMember>> getAll() const;

    [[nodiscard]] Status saveDirectory(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        ) const;

    QSqlDatabase& m_database;
    mutable std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    mutable QString m_engineDatabasePath;
};

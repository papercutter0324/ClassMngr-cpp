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
    explicit GsTeamRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
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

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    mutable std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    mutable QString m_engineDatabasePath;
};

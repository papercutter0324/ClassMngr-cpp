#pragma once

#include "core/result.h"
#include "domain/models/gs_team_member.h"

#include <QList>
#include <QSqlDatabase>

class GsTeamRepository
{
public:
    explicit GsTeamRepository(QSqlDatabase& database);

    [[nodiscard]] Result<QList<GsTeamMember>> getAll() const;

    [[nodiscard]] Status saveDirectory(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        );

private:
    QSqlDatabase& m_database;
};

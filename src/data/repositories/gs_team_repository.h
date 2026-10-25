#pragma once

#include "domain/models/gs_team_member.h"
#include "data/repositories/repository_error.h"

#include <QList>
#include <QSqlDatabase>

struct GsTeamBirthdayReadRecord final
{
    QString name;
    QString koreanName;
    QString position;
    QString birthday;
};

class GsTeamRepository
{
public:
    explicit GsTeamRepository(QSqlDatabase& database);

    [[nodiscard]] RepositoryResult<QList<GsTeamMember>> getAll() const;
    [[nodiscard]] RepositoryResult<QList<GsTeamBirthdayReadRecord>>
        loadBirthdayDirectoryReadRecords() const;

    [[nodiscard]] RepositoryStatus saveDirectory(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        );

private:
    QSqlDatabase& m_database;
};

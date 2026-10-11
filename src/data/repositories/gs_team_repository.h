#pragma once

#include "core/result.h"
#include "domain/models/gs_team_member.h"

#include <QList>
#include <QSqlDatabase>

struct GsTeamBirthdayReadRecord final
{
    QString name;
    QString koreanName;
    QString position;
    QString birthday;
};

enum class GsTeamDirectorySaveOutcomeCode
{
    Saved,
    Missing,
    Failure
};

struct GsTeamDirectorySaveOutcome final
{
    GsTeamDirectorySaveOutcomeCode code =
        GsTeamDirectorySaveOutcomeCode::Failure;
    QString diagnostic;
};

class GsTeamRepository
{
public:
    explicit GsTeamRepository(QSqlDatabase& database);

    [[nodiscard]] Result<QList<GsTeamMember>> getAll() const;
    [[nodiscard]] Result<QList<GsTeamBirthdayReadRecord>>
        loadBirthdayDirectoryReadRecords() const;

    [[nodiscard]] Status saveDirectory(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        );
    [[nodiscard]] GsTeamDirectorySaveOutcome saveDirectoryWithOutcome(
        const QList<GsTeamMember>& members,
        const QList<int>& deletedIds
        );

private:
    QSqlDatabase& m_database;
};

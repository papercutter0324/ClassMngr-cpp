#include "gs_team_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QSet>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>

#include <limits>
#include <utility>

namespace
{
QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

RepositoryError queryError(
    const QSqlQuery& query,
    const QString& action
    )
{
    return SqlQueryUtils::repositoryErrorFor(
        SqlQueryUtils::errorFor(query, action));
}

RepositoryStatus failedWrite(
    DatabaseTransaction& transaction,
    QSqlDatabase& database,
    RepositoryError error
    )
{
    if (!transaction.rollbackChecked())
    {
        error.code = RepositoryErrorCode::Technical;
        error.recoverableOverride.reset();
        const QString rollbackDetail = database.lastError().text().trimmed();
        error.diagnostic += rollbackDetail.isEmpty()
            ? QObject::tr(" (rollback failed)")
            : QObject::tr(" (rollback failed: %1)").arg(rollbackDetail);
    }

    return std::unexpected(std::move(error));
}
}

GsTeamRepository::GsTeamRepository(QSqlDatabase& database)
    : m_database(database)
{
}

RepositoryResult<QList<GsTeamMember>> GsTeamRepository::getAll() const
{
    QList<GsTeamMember> result;
    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
        SELECT id, name, korean_name, position, phone_number, birthday
        FROM gs_team
        ORDER BY CASE position
            WHEN 'Branch Manager' THEN 1
            WHEN 'M3' THEN 2
            WHEN 'M2' THEN 3
            WHEN 'M1' THEN 4
            WHEN 'C3' THEN 5
            WHEN 'C2' THEN 6
            WHEN 'C1' THEN 7
            ELSE 8
        END,
        CASE WHEN name='' THEN korean_name ELSE name END COLLATE NOCASE,
        id
    )"),
        QObject::tr("Loading GS Team directory")
        );
    if (!executed)
    {
        return std::unexpected(
            SqlQueryUtils::repositoryErrorFor(executed.error()));
    }

    while (query.next())
    {
        bool idConversionSucceeded = false;
        const qlonglong rawId = query.value(0).toLongLong(
            &idConversionSucceeded);
        if (!idConversionSucceeded
            || rawId <= 0
            || rawId > std::numeric_limits<int>::max())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading GS Team directory failed: member id %1 is outside "
                    "the supported range.").arg(query.value(0).toString())
            });
        }

        GsTeamMember member;
        member.id = static_cast<int>(rawId);
        member.name = query.value(1).toString();
        member.koreanName = query.value(2).toString();
        member.position = query.value(3).toString();
        member.phoneNumber = query.value(4).toString();
        member.birthday = query.value(5).toString();
        result.append(member);
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(queryError(
            query,
            QObject::tr("Loading GS Team directory")));
    }

    return result;
}

RepositoryResult<QList<GsTeamBirthdayReadRecord>>
GsTeamRepository::loadBirthdayDirectoryReadRecords() const
{
    QList<GsTeamBirthdayReadRecord> result;
    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
        SELECT name, korean_name, position, birthday
        FROM gs_team
        ORDER BY CASE position
            WHEN 'Branch Manager' THEN 1
            WHEN 'M3' THEN 2
            WHEN 'M2' THEN 3
            WHEN 'M1' THEN 4
            WHEN 'C3' THEN 5
            WHEN 'C2' THEN 6
            WHEN 'C1' THEN 7
            ELSE 8
        END,
        CASE WHEN name='' THEN korean_name ELSE name END COLLATE NOCASE,
        id
    )"),
        QObject::tr("Loading GS Team directory")
        );
    if (!executed)
    {
        return std::unexpected(
            SqlQueryUtils::repositoryErrorFor(executed.error()));
    }

    while (query.next())
    {
        result.append({
            query.value(QStringLiteral("name")).toString(),
            query.value(QStringLiteral("korean_name")).toString(),
            query.value(QStringLiteral("position")).toString(),
            query.value(QStringLiteral("birthday")).toString()
        });
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            query.lastError().text()
        });
    }

    return result;
}

RepositoryStatus GsTeamRepository::saveDirectory(
    const QList<GsTeamMember>& members,
    const QList<int>& deletedIds
    )
{
    QSet<QString> englishNames;
    QSet<QString> koreanNames;

    for (const GsTeamMember& member : members)
    {
        const QString english = normalizedName(member.name);
        const QString korean = normalizedName(member.koreanName);
        if (english.isEmpty() && korean.isEmpty())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Every GS Team member must have a name or Korean name.")
            });
        }
        if ((!english.isEmpty() && englishNames.contains(english))
            || (!korean.isEmpty() && koreanNames.contains(korean)))
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr("GS Team names must be unique.")
            });
        }
        if (!english.isEmpty()) englishNames.insert(english);
        if (!korean.isEmpty()) koreanNames.insert(korean);
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Unable to start the directory save transaction.")
        });
    }

    for (int id : deletedIds)
    {
        if (id <= 0) continue;
        QSqlQuery query(m_database);
        if (!query.prepare(QStringLiteral("DELETE FROM gs_team WHERE id=?")))
        {
            return failedWrite(transaction, m_database, queryError(
                query, QObject::tr("Deleting a GS Team member")));
        }
        query.addBindValue(id);
        if (!query.exec())
        {
            return failedWrite(transaction, m_database, queryError(
                query, QObject::tr("Deleting a GS Team member")));
        }
    }

    for (const GsTeamMember& source : members)
    {
        GsTeamMember member = source;
        member.name = member.name.simplified();
        member.koreanName = member.koreanName.simplified();
        member.position = member.position.trimmed();
        member.phoneNumber = member.phoneNumber.trimmed();
        member.birthday = member.birthday.trimmed();

        QSqlQuery query(m_database);
        if (member.id > 0)
        {
            if (!query.prepare(R"(
                UPDATE gs_team
                SET name=?, korean_name=?, position=?, phone_number=?, birthday=?
                WHERE id=?
            )"))
            {
                return failedWrite(transaction, m_database, queryError(
                    query, QObject::tr("Saving a GS Team member")));
            }
            query.addBindValue(member.name);
            query.addBindValue(member.koreanName);
            query.addBindValue(member.position);
            query.addBindValue(member.phoneNumber);
            query.addBindValue(member.birthday);
            query.addBindValue(member.id);
        }
        else
        {
            if (!query.prepare(R"(
                INSERT INTO gs_team
                    (name, korean_name, position, phone_number, birthday)
                VALUES (?, ?, ?, ?, ?)
            )"))
            {
                return failedWrite(transaction, m_database, queryError(
                    query, QObject::tr("Saving a GS Team member")));
            }
            query.addBindValue(member.name);
            query.addBindValue(member.koreanName);
            query.addBindValue(member.position);
            query.addBindValue(member.phoneNumber);
            query.addBindValue(member.birthday);
        }

        if (!query.exec())
        {
            return failedWrite(transaction, m_database, queryError(
                query, QObject::tr("Saving a GS Team member")));
        }
        if (member.id > 0 && query.numRowsAffected() == 0)
        {
            return failedWrite(
                transaction,
                m_database,
                RepositoryError{
                    RepositoryErrorCode::NotFound,
                    QObject::tr(
                        "Saving GS Team member id %1 failed: no matching record exists."
                        ).arg(member.id),
                    true
                });
        }
    }

    if (!transaction.commit())
    {
        return failedWrite(
            transaction,
            m_database,
            RepositoryError{
                RepositoryErrorCode::Technical,
                QObject::tr("Unable to commit the GS Team directory.")
            });
    }

    return {};
}

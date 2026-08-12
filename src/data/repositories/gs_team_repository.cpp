#include "gs_team_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

Status queryError(const QSqlQuery& query, const QString& action)
{
    return std::unexpected(
        SqlQueryUtils::errorFor(query, action).userMessage()
        );
}
}

GsTeamRepository::GsTeamRepository(QSqlDatabase& database)
    : m_database(database)
{
}

QList<GsTeamMember> GsTeamRepository::getAll() const
{
    QList<GsTeamMember> result;
    QSqlQuery query(m_database);

    if (!query.exec(R"(
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
    )"))
    {
        return result;
    }

    while (query.next())
    {
        GsTeamMember member;
        member.id = query.value(0).toInt();
        member.name = query.value(1).toString();
        member.koreanName = query.value(2).toString();
        member.position = query.value(3).toString();
        member.phoneNumber = query.value(4).toString();
        member.birthday = query.value(5).toString();
        result.append(member);
    }

    return result;
}

Status GsTeamRepository::saveDirectory(
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
            return std::unexpected(QObject::tr("Every GS Team member must have a name or Korean name."));
        }
        if ((!english.isEmpty() && englishNames.contains(english))
            || (!korean.isEmpty() && koreanNames.contains(korean)))
        {
            return std::unexpected(QObject::tr("GS Team names must be unique."));
        }
        if (!english.isEmpty()) englishNames.insert(english);
        if (!korean.isEmpty()) koreanNames.insert(korean);
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(QObject::tr("Unable to start the directory save transaction."));
    }

    for (int id : deletedIds)
    {
        if (id <= 0) continue;
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("DELETE FROM gs_team WHERE id=?"));
        query.addBindValue(id);
        if (!query.exec())
        {
            return queryError(query, QObject::tr("Deleting a GS Team member"));
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
            query.prepare(R"(
                UPDATE gs_team
                SET name=?, korean_name=?, position=?, phone_number=?, birthday=?
                WHERE id=?
            )");
            query.addBindValue(member.name);
            query.addBindValue(member.koreanName);
            query.addBindValue(member.position);
            query.addBindValue(member.phoneNumber);
            query.addBindValue(member.birthday);
            query.addBindValue(member.id);
        }
        else
        {
            query.prepare(R"(
                INSERT INTO gs_team
                    (name, korean_name, position, phone_number, birthday)
                VALUES (?, ?, ?, ?, ?)
            )");
            query.addBindValue(member.name);
            query.addBindValue(member.koreanName);
            query.addBindValue(member.position);
            query.addBindValue(member.phoneNumber);
            query.addBindValue(member.birthday);
        }

        if (!query.exec())
        {
            return queryError(query, QObject::tr("Saving a GS Team member"));
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(QObject::tr("Unable to commit the GS Team directory."));
    }

    return {};
}

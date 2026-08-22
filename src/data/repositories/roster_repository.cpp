#include "roster_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>

RosterRepository::RosterRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Status RosterRepository::saveRoster(
    int classId,
    const Roster& roster
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Saving roster failed: invalid class id %1.")
                .arg(classId)
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting roster save transaction failed for class id %1: %2")
                .arg(classId)
                .arg(m_database.lastError().text())
            );
    }

    const Status saved = writeRoster(classId, roster);
    if (!saved)
    {
        return saved;
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing roster save failed for class id %1: %2")
                .arg(classId)
                .arg(m_database.lastError().text())
            );
    }

    return {};
}

Status RosterRepository::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    )
{
    if (rosters.isEmpty())
    {
        return {};
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting roster batch save transaction failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    for (const auto& roster : rosters)
    {
        const Status saved = writeRoster(roster.first, roster.second);
        if (!saved)
        {
            return saved;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing roster batch save failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    return {};
}

Status RosterRepository::writeRoster(
    int classId,
    const Roster& roster
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Saving roster failed: invalid class id %1.")
                .arg(classId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare(
        "DELETE FROM roster_columns WHERE class_id=?"
        );

    query.addBindValue(classId);

    const QString identity = QObject::tr("class id %1").arg(classId);
    auto execute = [&](const QString& action) -> Status
    {
        const auto result = SqlQueryUtils::executePrepared(
            query, action, identity);
        return result
            ? Status{}
            : Status(std::unexpected(result.error().userMessage()));
    };

    Status statement = execute(QObject::tr("Deleting roster columns"));
    if (!statement)
    {
        return statement;
    }

    query.prepare(
        "DELETE FROM roster_data WHERE class_id=?"
        );

    query.addBindValue(classId);

    statement = execute(QObject::tr("Deleting roster data"));
    if (!statement)
    {
        return statement;
    }

    for (int column = 0; column < roster.columns.size(); ++column)
    {
        query.prepare(R"(
            INSERT INTO roster_columns (
                class_id,
                name,
                position,
                width
            )
            VALUES (?, ?, ?, ?)
        )");

        const int width =
            column < roster.columnWidths.size()
                ? roster.columnWidths[column]
                : 0;

        query.addBindValue(classId);
        query.addBindValue(roster.columns[column]);
        query.addBindValue(column);
        query.addBindValue(width);

        statement = execute(QObject::tr("Inserting roster column"));
        if (!statement)
        {
            return statement;
        }
    }

    for (int row = 0; row < roster.rows.size(); ++row)
    {
        const QStringList& rowValues =
            roster.rows[row];

        for (int column = 0; column < roster.columns.size(); ++column)
        {
            const QString value =
                column < rowValues.size()
                    ? rowValues[column]
                    : QString();

            if (value.isEmpty())
            {
                continue;
            }

            query.prepare(R"(
                INSERT INTO roster_data (
                    class_id,
                    row_index,
                    col_index,
                    value
                )
                VALUES (?, ?, ?, ?)
            )");

            query.addBindValue(classId);
            query.addBindValue(row);
            query.addBindValue(column);
            query.addBindValue(value);

            statement = execute(QObject::tr("Inserting roster data"));
            if (!statement)
            {
                return statement;
            }
        }
    }

    return {};
}

Result<Roster> RosterRepository::loadRoster(
    int classId
    )
{
    Roster roster;

    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading roster failed: invalid class id %1.")
                .arg(classId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT
            name,
            width
        FROM roster_columns
        WHERE class_id=?
        ORDER BY position, id
    )");

    query.addBindValue(classId);

    const QString identity = QObject::tr("class id %1").arg(classId);
    const auto loadedColumns = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading roster columns"),
        identity
        );
    if (!loadedColumns)
    {
        return std::unexpected(loadedColumns.error().userMessage());
    }

    while (query.next())
    {
        roster.columns.append(
            query.value("name").toString()
            );

        roster.columnWidths.append(
            query.value("width").toInt()
            );
    }

    if (roster.columns.isEmpty())
    {
        return roster;
    }

    query.prepare(R"(
        SELECT
            row_index,
            col_index,
            value
        FROM roster_data
        WHERE class_id=?
        ORDER BY row_index, col_index
    )");

    query.addBindValue(classId);

    const auto loadedData = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading roster data"),
        identity
        );
    if (!loadedData)
    {
        return std::unexpected(loadedData.error().userMessage());
    }

    while (query.next())
    {
        const int row =
            query.value("row_index").toInt();

        const int column =
            query.value("col_index").toInt();

        if (
            row < 0
            || column < 0
            || column >= roster.columns.size()
            )
        {
            continue;
        }

        while (roster.rows.size() <= row)
        {
            QStringList emptyRow;

            for (int index = 0; index < roster.columns.size(); ++index)
            {
                emptyRow.append(QString());
            }

            roster.rows.append(emptyRow);
        }

        roster.rows[row][column] =
            query.value("value").toString();
    }

    return roster;
}

Result<int> RosterRepository::getRosterStudentCount(
    int classId
    )
{
    const Result<Roster> roster =
        loadRoster(classId);
    if (!roster)
    {
        return std::unexpected(roster.error());
    }

    const int englishColumn =
        roster->columns.indexOf(
            QStringLiteral("English")
            );

    const int koreanColumn =
        roster->columns.indexOf(
            QStringLiteral("Korean")
            );

    if (englishColumn < 0 && koreanColumn < 0)
    {
        return 0;
    }

    int count = 0;

    for (const QStringList& row : roster->rows)
    {
        const bool hasEnglish =
            englishColumn >= 0
            && englishColumn < row.size()
            && !row[englishColumn].trimmed().isEmpty();

        const bool hasKorean =
            koreanColumn >= 0
            && koreanColumn < row.size()
            && !row[koreanColumn].trimmed().isEmpty();

        if (hasEnglish || hasKorean)
        {
            ++count;
        }
    }

    return count;
}

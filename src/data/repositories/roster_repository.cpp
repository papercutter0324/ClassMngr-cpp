#include "roster_repository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

RosterRepository::RosterRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

void RosterRepository::saveRoster(
    int classId,
    const Roster& roster
    )
{
    if (classId <= 0)
    {
        return;
    }

    if (!m_database.transaction())
    {
        qWarning()
            << "Failed to start roster save transaction:"
            << m_database.lastError().text();

        return;
    }

    if (!writeRoster(classId, roster))
    {
        if (!m_database.rollback())
        {
            qWarning()
                << "Failed to roll back roster save transaction:"
                << m_database.lastError().text();
        }

        return;
    }

    if (!m_database.commit())
    {
        qWarning()
            << "Failed to commit roster save transaction:"
            << m_database.lastError().text();

        if (!m_database.rollback())
        {
            qWarning()
                << "Failed to roll back failed roster save commit:"
                << m_database.lastError().text();
        }
    }
}

bool RosterRepository::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    )
{
    if (rosters.isEmpty())
    {
        return true;
    }

    if (!m_database.transaction())
    {
        qWarning()
            << "Failed to start roster batch save transaction:"
            << m_database.lastError().text();

        return false;
    }

    for (const auto& roster : rosters)
    {
        if (!writeRoster(roster.first, roster.second))
        {
            if (!m_database.rollback())
            {
                qWarning()
                    << "Failed to roll back roster batch save transaction:"
                    << m_database.lastError().text();
            }

            return false;
        }
    }

    if (!m_database.commit())
    {
        qWarning()
            << "Failed to commit roster batch save transaction:"
            << m_database.lastError().text();

        if (!m_database.rollback())
        {
            qWarning()
                << "Failed to roll back failed roster batch save commit:"
                << m_database.lastError().text();
        }

        return false;
    }

    return true;
}

bool RosterRepository::writeRoster(
    int classId,
    const Roster& roster
    )
{
    if (classId <= 0)
    {
        return false;
    }

    QSqlQuery query(m_database);

    query.prepare(
        "DELETE FROM roster_columns WHERE class_id=?"
        );

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Deleting roster_columns failed while saving roster:"
            << query.lastError().text();

        return false;
    }

    query.prepare(
        "DELETE FROM roster_data WHERE class_id=?"
        );

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Deleting roster_data failed while saving roster:"
            << query.lastError().text();

        return false;
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

        if (!query.exec())
        {
            qWarning()
                << "Inserting roster_columns failed while saving roster:"
                << query.lastError().text();

            return false;
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

            if (!query.exec())
            {
                qWarning()
                    << "Inserting roster_data failed while saving roster:"
                    << query.lastError().text();

                return false;
            }
        }
    }

    return true;
}

Roster RosterRepository::loadRoster(
    int classId
    )
{
    Roster roster;

    if (classId <= 0)
    {
        return roster;
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

    if (!query.exec())
    {
        qWarning()
            << "Failed to load roster columns:"
            << query.lastError().text();

        return roster;
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

    if (!query.exec())
    {
        qWarning()
            << "Failed to load roster data:"
            << query.lastError().text();

        return roster;
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

int RosterRepository::getRosterStudentCount(
    int classId
    )
{
    const Roster roster =
        loadRoster(classId);

    const int englishColumn =
        roster.columns.indexOf(
            QStringLiteral("English")
            );

    const int koreanColumn =
        roster.columns.indexOf(
            QStringLiteral("Korean")
            );

    if (englishColumn < 0 && koreanColumn < 0)
    {
        return 0;
    }

    int count = 0;

    for (const QStringList& row : roster.rows)
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

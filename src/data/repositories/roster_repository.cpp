#include "roster_repository.h"

#include "core/startup_profiler.h"
#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QHash>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>

#include <vector>

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

Result<Roster> RosterRepository::loadRosterForOutput(
    const int classId,
    const QStringList& requestedColumns,
    const std::size_t maxRows,
    const std::size_t maxCells,
    const std::size_t maxTextBytes
    )
{
    const QString identity = QObject::tr("class id %1").arg(classId);
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading roster output failed: invalid class id %1.")
                .arg(classId)
            );
    }
    if (requestedColumns.size()
            > static_cast<qsizetype>(kRosterRepositoryOutputMaxColumns)
        || maxRows > kRosterRepositoryOutputMaxRows
        || maxCells > kRosterRepositoryOutputMaxCells
        || maxTextBytes > kRosterRepositoryOutputMaxTextBytes)
    {
        return std::unexpected(
            QObject::tr("Loading roster output failed: a requested limit exceeds its bound.")
            );
    }

    for (qsizetype index = 0; index < requestedColumns.size(); ++index)
    {
        const QString& column = requestedColumns.at(index);
        if (column.size()
                > static_cast<qsizetype>(
                    kRosterRepositoryOutputMaxColumnNameBytes
                    )
            || column.trimmed().isEmpty()
            || column.toUtf8().size()
                > static_cast<qsizetype>(
                    kRosterRepositoryOutputMaxColumnNameBytes
                    ))
        {
            return std::unexpected(
                QObject::tr("Loading roster output failed: a column name is blank or too long.")
                );
        }
        for (qsizetype other = 0; other < index; ++other)
        {
            if (requestedColumns.at(other).compare(
                    column,
                    Qt::CaseInsensitive
                    ) == 0)
            {
                return std::unexpected(
                    QObject::tr("Loading roster output failed: requested columns are duplicated.")
                    );
            }
        }
    }

    struct RequestedColumn final
    {
        QString name;
        int sourceIndex = -1;
        std::size_t textBytes = 0;
    };

    std::vector<RequestedColumn> selectedColumns;
    selectedColumns.reserve(
        static_cast<std::size_t>(requestedColumns.size())
        );
    for (const QString& column : requestedColumns)
    {
        selectedColumns.push_back(
            {
                column,
                -1,
                static_cast<std::size_t>(column.toUtf8().size())
            }
            );
    }

    QSqlQuery columnsQuery(m_database);
    columnsQuery.setForwardOnly(true);
    columnsQuery.prepare(R"(
        SELECT
            substr(name, 1, 257) AS bounded_name,
            length(CAST(name AS BLOB)) AS name_bytes
        FROM roster_columns
        WHERE class_id=?
        ORDER BY position, id
        LIMIT ?
    )");
    columnsQuery.addBindValue(classId);
    columnsQuery.addBindValue(4'097);

    const auto loadedColumns = SqlQueryUtils::executePrepared(
        columnsQuery,
        QObject::tr("Loading bounded roster output columns"),
        identity
        );
    if (!loadedColumns)
    {
        return std::unexpected(loadedColumns.error().userMessage());
    }

    int sourceColumnIndex = 0;
    while (columnsQuery.next())
    {
        if (sourceColumnIndex >= 4'096)
        {
            return std::unexpected(
                QObject::tr("Loading roster output failed: roster schema exceeds its safe column limit.")
                );
        }

        bool byteLengthValid = false;
        const qulonglong rawNameBytes =
            columnsQuery.value("name_bytes").toULongLong(&byteLengthValid);
        if (!byteLengthValid)
        {
            return std::unexpected(
                QObject::tr("Loading roster output failed: a column name has invalid size metadata.")
                );
        }

        if (rawNameBytes <= kRosterRepositoryOutputMaxColumnNameBytes)
        {
            const QString sourceName =
                columnsQuery.value("bounded_name").toString();
            for (RequestedColumn& selected : selectedColumns)
            {
                if (selected.sourceIndex < 0
                    && sourceName.compare(
                        selected.name,
                        Qt::CaseInsensitive
                        ) == 0)
                {
                    selected.sourceIndex = sourceColumnIndex;
                }
            }
        }
        ++sourceColumnIndex;
    }
    columnsQuery.finish();

    Roster roster;
    std::size_t totalTextBytes = 0;
    QList<int> sourceIndexes;
    for (const RequestedColumn& selected : selectedColumns)
    {
        if (selected.sourceIndex < 0)
        {
            continue;
        }
        if (selected.textBytes > maxTextBytes
            || totalTextBytes > maxTextBytes - selected.textBytes)
        {
            return std::unexpected(
                QObject::tr("Loading roster output failed: column names exceed the text byte limit.")
                );
        }
        totalTextBytes += selected.textBytes;
        sourceIndexes.append(selected.sourceIndex);
        roster.columns.append(selected.name);
        roster.columnWidths.append(0);
    }

    if (sourceIndexes.isEmpty())
    {
        return roster;
    }

    QStringList placeholders;
    for (qsizetype index = 0; index < sourceIndexes.size(); ++index)
    {
        placeholders.append(QStringLiteral("?"));
    }
    const QString indexPlaceholders = placeholders.join(QStringLiteral(", "));

    QSqlQuery maximumRowQuery(m_database);
    maximumRowQuery.setForwardOnly(true);
    maximumRowQuery.prepare(
        QStringLiteral(
            "SELECT MAX(row_index) FROM roster_data "
            "WHERE class_id=? AND row_index>=0 AND col_index IN (%1)"
            ).arg(indexPlaceholders)
        );
    maximumRowQuery.addBindValue(classId);
    for (const int sourceIndex : sourceIndexes)
    {
        maximumRowQuery.addBindValue(sourceIndex);
    }

    const auto loadedMaximumRow = SqlQueryUtils::executePrepared(
        maximumRowQuery,
        QObject::tr("Sizing bounded roster output rows"),
        identity
        );
    if (!loadedMaximumRow)
    {
        return std::unexpected(loadedMaximumRow.error().userMessage());
    }
    if (!maximumRowQuery.next())
    {
        return std::unexpected(
            QObject::tr("Loading roster output failed: row count could not be read.")
            );
    }

    const QVariant maximumRowValue = maximumRowQuery.value(0);
    std::size_t rowCount = 0;
    if (!maximumRowValue.isNull())
    {
        bool rowIndexValid = false;
        const qlonglong maximumRowIndex =
            maximumRowValue.toLongLong(&rowIndexValid);
        if (!rowIndexValid || maximumRowIndex < 0
            || static_cast<qulonglong>(maximumRowIndex) >= maxRows)
        {
            return std::unexpected(
                QObject::tr("Loading roster output failed: roster rows exceed the requested limit.")
                );
        }
        rowCount = static_cast<std::size_t>(maximumRowIndex) + 1;
    }
    maximumRowQuery.finish();

    const std::size_t columnCount =
        static_cast<std::size_t>(roster.columns.size());
    if (columnCount != 0 && rowCount > maxCells / columnCount)
    {
        return std::unexpected(
            QObject::tr("Loading roster output failed: roster cells exceed the requested limit.")
            );
    }
    const std::size_t cellCount = rowCount * columnCount;

    roster.rows.reserve(static_cast<qsizetype>(rowCount));
    for (std::size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(columnCount));
        for (std::size_t column = 0; column < columnCount; ++column)
        {
            row.append(QString());
        }
        roster.rows.append(std::move(row));
    }

    QHash<int, int> outputIndexesBySourceIndex;
    outputIndexesBySourceIndex.reserve(sourceIndexes.size());
    for (qsizetype outputIndex = 0;
         outputIndex < sourceIndexes.size();
         ++outputIndex)
    {
        outputIndexesBySourceIndex.insert(
            sourceIndexes.at(outputIndex),
            static_cast<int>(outputIndex)
            );
    }

    QSqlQuery valuesQuery(m_database);
    valuesQuery.setForwardOnly(true);
    valuesQuery.prepare(
        QStringLiteral(
            "SELECT row_index, col_index, "
            "substr(value, 1, %1) AS bounded_value, "
            "length(CAST(value AS BLOB)) AS value_bytes "
            "FROM roster_data "
            "WHERE class_id=? AND row_index>=0 AND col_index IN (%2) "
            "ORDER BY row_index, col_index LIMIT ?"
            )
            .arg(
                static_cast<qulonglong>(
                    kRosterRepositoryOutputMaxCellBytes + 1
                    )
                )
            .arg(indexPlaceholders)
        );
    valuesQuery.addBindValue(classId);
    for (const int sourceIndex : sourceIndexes)
    {
        valuesQuery.addBindValue(sourceIndex);
    }
    valuesQuery.addBindValue(static_cast<qulonglong>(cellCount + 1));

    const auto loadedValues = SqlQueryUtils::executePrepared(
        valuesQuery,
        QObject::tr("Loading bounded roster output values"),
        identity
        );
    if (!loadedValues)
    {
        return std::unexpected(loadedValues.error().userMessage());
    }

    std::size_t loadedCellCount = 0;
    while (valuesQuery.next())
    {
        if (loadedCellCount >= cellCount)
        {
            return std::unexpected(
                QObject::tr("Loading roster output failed: stored cells exceed their bounded matrix.")
                );
        }
        ++loadedCellCount;

        bool byteLengthValid = false;
        const qulonglong cellBytes =
            valuesQuery.value("value_bytes").toULongLong(&byteLengthValid);
        if (!byteLengthValid
            || cellBytes > kRosterRepositoryOutputMaxCellBytes
            || cellBytes > maxTextBytes - totalTextBytes)
        {
            return std::unexpected(
                QObject::tr("Loading roster output failed: a cell exceeds the text byte limit.")
                );
        }
        totalTextBytes += static_cast<std::size_t>(cellBytes);

        bool rowIndexValid = false;
        bool sourceIndexValid = false;
        const qlonglong rowIndex =
            valuesQuery.value("row_index").toLongLong(&rowIndexValid);
        const int sourceIndex =
            valuesQuery.value("col_index").toInt(&sourceIndexValid);
        const auto outputIndex =
            outputIndexesBySourceIndex.constFind(sourceIndex);
        if (!rowIndexValid || rowIndex < 0
            || static_cast<qulonglong>(rowIndex) >= rowCount
            || !sourceIndexValid
            || outputIndex == outputIndexesBySourceIndex.cend())
        {
            continue;
        }

        roster.rows[static_cast<qsizetype>(rowIndex)][*outputIndex] =
            valuesQuery.value("bounded_value").toString();
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

    int cellCount = 0;
    for (const QStringList& row : roster->rows)
    {
        cellCount += row.size();
    }

    if (englishColumn < 0 && koreanColumn < 0)
    {
        StartupProfiler::recordSubPrepRosterQuery(
            classId,
            roster->columns.size(),
            roster->rows.size(),
            cellCount,
            0
            );
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

    StartupProfiler::recordSubPrepRosterQuery(
        classId,
        roster->columns.size(),
        roster->rows.size(),
        cellCount,
        count
        );

    return count;
}

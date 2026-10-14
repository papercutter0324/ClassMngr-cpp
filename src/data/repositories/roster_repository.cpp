#include "roster_repository.h"

#include "core/startup_profiler.h"
#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QHash>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QVariant>

#include <algorithm>
#include <limits>
#include <optional>
#include <vector>

namespace
{

std::optional<int> checkedRosterWidth(
    const QVariant& widthValue,
    const QString& storageType
    )
{
    if (widthValue.isNull())
    {
        return 0;
    }

    if (storageType != QStringLiteral("integer"))
    {
        return widthValue.toInt();
    }

    bool valid = false;
    const qlonglong width = widthValue.toLongLong(&valid);
    if (!valid
        || width < std::numeric_limits<int>::min()
        || width > std::numeric_limits<int>::max())
    {
        return std::nullopt;
    }

    return static_cast<int>(width);
}

} // namespace

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

RosterRepository::SaveResult RosterRepository::saveRosterWithParentCheck(
    const int classId,
    const Roster& roster
    )
{
    if (classId <= 0)
    {
        return std::unexpected(SaveFailure{
            .kind = SaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Saving roster failed: invalid class id %1."
                ).arg(classId)
        });
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    DatabaseTransaction transaction(
        m_database,
        DatabaseTransaction::Mode::Immediate
        );
    if (!transaction.started())
    {
        return std::unexpected(SaveFailure{
            .kind = SaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Starting roster save transaction failed for %1: %2"
                ).arg(identity, transaction.startError())
        });
    }

    bool parentClassExists = false;
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "SELECT 1 FROM classes WHERE id=?"
            ));
        query.addBindValue(classId);

        const auto loaded = SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Checking roster parent class"),
            identity
            );
        if (!loaded)
        {
            return std::unexpected(SaveFailure{
                .kind = SaveFailure::Kind::Technical,
                .diagnostic = loaded.error().userMessage()
            });
        }

        parentClassExists = query.next();
        const QSqlError fetchError = query.lastError();
        query.finish();
        if (!parentClassExists && fetchError.isValid())
        {
            return std::unexpected(SaveFailure{
                .kind = SaveFailure::Kind::Technical,
                .diagnostic = QObject::tr(
                    "Checking roster parent class failed for %1: %2"
                    ).arg(identity, fetchError.text())
            });
        }
    }

    if (!parentClassExists)
    {
        if (!transaction.rollbackChecked())
        {
            return std::unexpected(SaveFailure{
                .kind = SaveFailure::Kind::Technical,
                .diagnostic = QObject::tr(
                    "Rolling back roster save failed for %1: %2"
                    ).arg(identity, m_database.lastError().text())
            });
        }

        return std::unexpected(SaveFailure{
            .kind = SaveFailure::Kind::ParentClassMissing,
            .diagnostic = QObject::tr(
                "Saving roster failed: parent class id %1 was not found."
                ).arg(classId)
        });
    }

    const Status saved = writeRoster(classId, roster);
    if (!saved)
    {
        return std::unexpected(SaveFailure{
            .kind = SaveFailure::Kind::Technical,
            .diagnostic = saved.error()
        });
    }

    if (!transaction.commit())
    {
        return std::unexpected(SaveFailure{
            .kind = SaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Committing roster save failed for %1: %2"
                ).arg(identity, m_database.lastError().text())
        });
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
    auto loaded = loadRosterDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return std::move(loaded->roster);
}

Result<RosterReadOutcome> RosterRepository::loadRosterDetailed(
    int classId
    )
{
    RosterReadOutcome outcome;

    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading roster failed: invalid class id %1.")
                .arg(classId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        WITH requested(class_id) AS (VALUES (?))
        SELECT
            requested.class_id AS requested_class_id,
            classes.id AS parent_class_id,
            roster_columns.id AS roster_column_id,
            roster_columns.name,
            roster_columns.width,
            typeof(roster_columns.width) AS width_storage_type,
            roster_columns.position,
            typeof(roster_columns.position) AS position_storage_type
        FROM requested
        LEFT JOIN classes ON classes.id = requested.class_id
        LEFT JOIN roster_columns
            ON roster_columns.class_id = requested.class_id
        ORDER BY roster_columns.position, roster_columns.id
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

    const bool parentRowFound = query.next();
    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading roster columns"),
            {},
            identity
            ).userMessage());
    }

    if (!parentRowFound)
    {
        return std::unexpected(QObject::tr(
            "Loading roster columns failed: no result was returned for %1."
            ).arg(identity));
    }

    outcome.parentMissing = query.value("parent_class_id").isNull();

    do
    {
        // The requested-ID join keeps orphan column rows available to the
        // legacy wrapper while parent provenance is derived independently.
        if (query.value("roster_column_id").isNull())
        {
            continue;
        }

        bool positionValid = false;
        const qlonglong position =
            query.value("position").toLongLong(&positionValid);
        if (
            !positionValid
            || query.value("position_storage_type").toString()
                != QStringLiteral("integer")
            || position < 0
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster columns failed for %1: invalid position."
                    ).arg(identity)
                );
        }

        outcome.roster.columns.append(
            query.value("name").toString()
            );

        const QVariant widthValue = query.value("width");
        const auto width = checkedRosterWidth(
            widthValue,
            query.value("width_storage_type").toString()
            );
        if (!width)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster columns failed for class id %1: width %2 is outside the supported int range."
                    )
                    .arg(classId)
                    .arg(widthValue.toString())
                );
        }
        outcome.roster.columnWidths.append(*width);
    }
    while (query.next());

    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading roster columns"),
            {},
            identity
            ).userMessage());
    }

    if (outcome.roster.columns.isEmpty())
    {
        return outcome;
    }

    query.prepare(R"(
        SELECT
            row_index,
            typeof(row_index) AS row_index_storage_type,
            col_index,
            typeof(col_index) AS col_index_storage_type,
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
        if (
            query.value("row_index_storage_type").toString()
                != QStringLiteral("integer")
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: invalid row index."
                    ).arg(identity)
                );
        }

        bool rowIndexValid = false;
        const qlonglong rowIndex =
            query.value("row_index").toLongLong(&rowIndexValid);
        if (!rowIndexValid)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: invalid row index."
                    ).arg(identity)
                );
        }
        if (rowIndex < 0)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: row index %2 is negative."
                    ).arg(identity).arg(rowIndex)
                );
        }
        if (rowIndex > std::numeric_limits<int>::max())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: row index %2 exceeds the supported range."
                    ).arg(identity).arg(rowIndex)
                );
        }
        const int row = static_cast<int>(rowIndex);

        if (
            query.value("col_index_storage_type").toString()
                != QStringLiteral("integer")
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: invalid column index."
                    ).arg(identity)
                );
        }

        bool columnIndexValid = false;
        const qlonglong columnIndex =
            query.value("col_index").toLongLong(&columnIndexValid);
        if (!columnIndexValid)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: invalid column index."
                    ).arg(identity)
                );
        }

        if (
            columnIndex < 0
            || columnIndex >= outcome.roster.columns.size()
            )
        {
            continue;
        }
        const int column = static_cast<int>(columnIndex);

        while (outcome.roster.rows.size() <= row)
        {
            QStringList emptyRow;

            for (
                int index = 0;
                index < outcome.roster.columns.size();
                ++index
                )
            {
                emptyRow.append(QString());
            }

            outcome.roster.rows.append(emptyRow);
        }

        outcome.roster.rows[row][column] =
            query.value("value").toString();
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading roster data"),
            {},
            identity
            ).userMessage());
    }

    return outcome;
}

Result<RosterRepository::SpeakingEvaluationRosterNamesReadRecord>
RosterRepository::loadSpeakingEvaluationRosterNames(
    const int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Loading Speaking Evaluation roster names failed: "
                "invalid class id %1."
                ).arg(classId)
            );
    }

    SpeakingEvaluationRosterNamesReadRecord record;
    int englishColumn = -1;
    int koreanColumn = -1;
    int columnCount = 0;
    QSqlQuery columnsQuery(m_database);
    columnsQuery.prepare(R"(
        SELECT name
        FROM roster_columns
        WHERE class_id=?
        ORDER BY position, id
    )");
    columnsQuery.addBindValue(classId);

    const QString identity = QObject::tr("class id %1").arg(classId);
    const auto loadedColumns = SqlQueryUtils::executePrepared(
        columnsQuery,
        QObject::tr("Loading Speaking Evaluation roster name columns"),
        identity
        );
    if (!loadedColumns)
    {
        return std::unexpected(loadedColumns.error().userMessage());
    }

    while (columnsQuery.next())
    {
        const QString columnName = columnsQuery.value(0).toString();
        if (
            englishColumn < 0
            && columnName.compare(
                QStringLiteral("English"), Qt::CaseInsensitive) == 0
            )
        {
            englishColumn = columnCount;
        }
        if (
            koreanColumn < 0
            && columnName.compare(
                QStringLiteral("Korean"), Qt::CaseInsensitive) == 0
            )
        {
            koreanColumn = columnCount;
        }
        ++columnCount;
    }
    if (columnsQuery.lastError().isValid())
    {
        return std::unexpected(
            QObject::tr(
                "Loading Speaking Evaluation roster name columns failed "
                "for %1: %2"
                ).arg(identity, columnsQuery.lastError().text())
            );
    }

    record.hasEnglishColumn = englishColumn >= 0;
    record.hasKoreanColumn = koreanColumn >= 0;
    if (columnCount == 0)
    {
        return record;
    }

    QSqlQuery rowsQuery(m_database);
    rowsQuery.prepare(R"(
        WITH valid_rows AS (
            SELECT
                row_index,
                MAX(CASE WHEN typeof(col_index) <> 'integer' THEN 1 ELSE 0 END)
                    AS has_non_integer_column_index,
                MAX(CASE WHEN typeof(row_index) <> 'integer' THEN 1 ELSE 0 END)
                    AS has_non_integer_row_index
            FROM roster_data
            WHERE class_id=?
              AND row_index >= 0
              AND col_index >= 0
              AND col_index < ?
            GROUP BY row_index
        )
        SELECT
            valid_rows.row_index,
            valid_rows.has_non_integer_column_index,
            valid_rows.has_non_integer_row_index,
            english.value AS english_name,
            korean.value AS korean_name
        FROM valid_rows
        LEFT JOIN roster_data AS english
            ON english.class_id=?
           AND english.row_index=valid_rows.row_index
           AND english.col_index=?
        LEFT JOIN roster_data AS korean
            ON korean.class_id=?
           AND korean.row_index=valid_rows.row_index
           AND korean.col_index=?
        ORDER BY valid_rows.row_index
    )");
    rowsQuery.addBindValue(classId);
    rowsQuery.addBindValue(columnCount);
    rowsQuery.addBindValue(classId);
    rowsQuery.addBindValue(englishColumn);
    rowsQuery.addBindValue(classId);
    rowsQuery.addBindValue(koreanColumn);

    const auto loadedRows = SqlQueryUtils::executePrepared(
        rowsQuery,
        QObject::tr("Loading Speaking Evaluation roster names"),
        identity
        );
    if (!loadedRows)
    {
        return std::unexpected(loadedRows.error().userMessage());
    }

    while (rowsQuery.next())
    {
        if (rowsQuery.value(1).toInt() != 0)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading Speaking Evaluation roster names failed for %1: a column index is not stored as an integer."
                    ).arg(identity)
                );
        }
        if (rowsQuery.value(2).toInt() != 0)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading Speaking Evaluation roster names failed for %1: a row index is not stored as an integer."
                    ).arg(identity)
                );
        }

        bool rowIndexValid = false;
        const qlonglong sourceRowIndex =
            rowsQuery.value(0).toLongLong(&rowIndexValid);
        if (!rowIndexValid)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading Speaking Evaluation roster names failed for %1: invalid row index."
                    ).arg(identity)
                );
        }
        if (sourceRowIndex < 0)
        {
            continue;
        }
        if (sourceRowIndex > std::numeric_limits<int>::max())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading Speaking Evaluation roster names failed for %1: row index %2 exceeds the supported range."
                    ).arg(identity).arg(sourceRowIndex)
                );
        }
        const int rowIndex = static_cast<int>(sourceRowIndex);

        while (record.rows.size() <= rowIndex)
        {
            record.rows.append(
                SpeakingEvaluationRosterNamePairReadRecord{}
                );
        }

        record.rows[rowIndex].englishName =
            rowsQuery.value(3).toString();
        record.rows[rowIndex].koreanName =
            rowsQuery.value(4).toString();
    }
    if (rowsQuery.lastError().isValid())
    {
        return std::unexpected(
            QObject::tr(
                "Loading Speaking Evaluation roster names failed for %1: %2"
                ).arg(identity, rowsQuery.lastError().text())
            );
    }

    return record;
}

Result<QList<RosterRepository::FullRosterReadRecord>>
RosterRepository::loadFullRostersImpl(
    const QList<int>& classIds,
    TemplatePrintBatchReadMetrics* templatePrintMetrics,
    const bool templatePrintContext
    )
{
    if (classIds.isEmpty())
    {
        return QList<FullRosterReadRecord>{};
    }

    if (templatePrintMetrics)
    {
        ++templatePrintMetrics->callCount;
        templatePrintMetrics->requestedClassCount += classIds.size();
    }

    QList<FullRosterReadRecord> records;
    records.reserve(classIds.size());
    QHash<int, qsizetype> requestIndexByClassId;
    requestIndexByClassId.reserve(classIds.size());
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds.at(index);
        if (classId <= 0 || requestIndexByClassId.contains(classId))
        {
            return std::unexpected(templatePrintContext
                ? QObject::tr(
                    "Loading template print rosters failed: class ids must be positive and unique.")
                : QObject::tr(
                    "Loading full rosters failed: class ids must be positive and unique."));
        }

        requestIndexByClassId.insert(classId, index);
        requestedValues.append(
            QStringLiteral("(%1, %2)").arg(classId).arg(index)
            );
        records.append({.classId = classId});
    }

    QSqlQuery columnsQuery(m_database);
    columnsQuery.setForwardOnly(true);
    if (templatePrintMetrics)
    {
        ++templatePrintMetrics->columnStatementCount;
    }
    const QString columnsQueryText = QStringLiteral(R"(
        WITH requested(class_id, request_order) AS (VALUES %1)
        SELECT requested.class_id AS requested_class_id,
               requested.request_order AS request_order,
               roster_columns.id AS column_id,
               roster_columns.name AS name,
               roster_columns.width AS width,
               typeof(roster_columns.width) AS width_storage_type
        FROM requested
        LEFT JOIN roster_columns
            ON roster_columns.class_id=requested.class_id
        ORDER BY
            requested.request_order,
            roster_columns.position,
            roster_columns.id
    )").arg(requestedValues.join(QStringLiteral(", ")));
    const QString columnsAction = templatePrintContext
        ? QObject::tr("Loading template print roster columns")
        : QObject::tr("Loading full roster columns");
    const auto loadedColumns = SqlQueryUtils::execute(
        columnsQuery,
        columnsQueryText,
        columnsAction,
        QObject::tr("%1 classes").arg(classIds.size())
        );
    if (!loadedColumns)
    {
        return std::unexpected(loadedColumns.error().userMessage());
    }

    bool invalidColumnIdentity = false;
    while (columnsQuery.next())
    {
        const int classId = columnsQuery.value("requested_class_id").toInt();
        const int requestOrder = columnsQuery.value("request_order").toInt();
        if (requestOrder < 0
            || requestOrder >= classIds.size()
            || classIds.at(requestOrder) != classId
            || !requestIndexByClassId.contains(classId))
        {
            invalidColumnIdentity = true;
            break;
        }

        if (columnsQuery.value("column_id").isNull())
        {
            continue;
        }

        Roster& roster = records[requestOrder].roster;
        const QVariant widthValue = columnsQuery.value("width");
        const auto width = checkedRosterWidth(
            widthValue,
            columnsQuery.value("width_storage_type").toString()
            );
        if (!width)
        {
            return std::unexpected(
                (templatePrintContext
                    ? QObject::tr(
                        "Loading template print roster columns failed for class id %1: width %2 is outside the supported int range."
                        )
                    : QObject::tr(
                        "Loading full roster columns failed for class id %1: width %2 is outside the supported int range."
                        ))
                    .arg(classId)
                    .arg(widthValue.toString())
                );
        }
        roster.columns.append(columnsQuery.value("name").toString());
        roster.columnWidths.append(*width);
    }
    if (columnsQuery.lastError().isValid())
    {
        return std::unexpected(
            (templatePrintContext
                ? QObject::tr("Reading template print roster columns failed: %1")
                : QObject::tr("Reading full roster columns failed: %1"))
                .arg(columnsQuery.lastError().text())
            );
    }
    if (invalidColumnIdentity)
    {
        return std::unexpected(templatePrintContext
            ? QObject::tr(
                "Loading template print roster columns returned an invalid class identity.")
            : QObject::tr(
                "Loading full roster columns returned an invalid class identity."));
    }

    bool anyColumns = false;
    QStringList cellRequestValues;
    cellRequestValues.reserve(classIds.size());
    for (qsizetype index = 0; index < records.size(); ++index)
    {
        const FullRosterReadRecord& record = records.at(index);
        if (!record.roster.columns.isEmpty())
        {
            anyColumns = true;
        }
        cellRequestValues.append(
            QStringLiteral("(%1, %2, %3)")
                .arg(record.classId)
                .arg(index)
                .arg(record.roster.columns.size())
            );
    }

    if (!anyColumns)
    {
        return records;
    }

    QSqlQuery cellsQuery(m_database);
    cellsQuery.setForwardOnly(true);
    if (templatePrintMetrics)
    {
        ++templatePrintMetrics->cellStatementCount;
    }
    const QString cellsQueryText = QStringLiteral(R"(
        WITH requested(class_id, request_order, column_count) AS (VALUES %1)
        SELECT requested.class_id AS requested_class_id,
               requested.request_order AS request_order,
               roster_data.row_index AS row_index,
               typeof(roster_data.row_index) AS row_index_storage_type,
               roster_data.col_index AS col_index,
               typeof(roster_data.col_index) AS col_index_storage_type,
               roster_data.value AS value
        FROM requested
        INNER JOIN roster_data
            ON roster_data.class_id=requested.class_id
        WHERE roster_data.row_index >= 0
            AND roster_data.col_index >= 0
            AND roster_data.col_index < requested.column_count
        ORDER BY
            requested.request_order,
            roster_data.row_index,
            roster_data.col_index
    )").arg(cellRequestValues.join(QStringLiteral(", ")));
    const QString cellsAction = templatePrintContext
        ? QObject::tr("Loading template print roster cells")
        : QObject::tr("Loading full roster cells");
    const auto loadedCells = SqlQueryUtils::execute(
        cellsQuery,
        cellsQueryText,
        cellsAction,
        QObject::tr("%1 classes").arg(classIds.size())
        );
    if (!loadedCells)
    {
        return std::unexpected(loadedCells.error().userMessage());
    }

    bool invalidCellIdentity = false;
    while (cellsQuery.next())
    {
        const int classId = cellsQuery.value("requested_class_id").toInt();
        const int requestOrder = cellsQuery.value("request_order").toInt();
        if (
            cellsQuery.value("col_index_storage_type").toString()
                != QStringLiteral("integer")
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for class id %1: invalid column index."
                    ).arg(classId)
                );
        }
        const int columnIndex = cellsQuery.value("col_index").toInt();
        if (requestOrder < 0
            || requestOrder >= classIds.size()
            || classIds.at(requestOrder) != classId
            || !requestIndexByClassId.contains(classId)
            || columnIndex < 0
            || columnIndex >= records[requestOrder].roster.columns.size())
        {
            invalidCellIdentity = true;
            break;
        }

        const QString rosterIdentity =
            QObject::tr("class id %1").arg(classId);
        bool rowIndexValid = false;
        if (
            cellsQuery.value("row_index_storage_type").toString()
                != QStringLiteral("integer")
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: invalid row index."
                    ).arg(rosterIdentity)
                );
        }
        const qlonglong sourceRowIndex =
            cellsQuery.value("row_index").toLongLong(&rowIndexValid);
        if (!rowIndexValid)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: invalid row index."
                    ).arg(rosterIdentity)
                );
        }
        if (sourceRowIndex < 0)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: row index %2 is negative."
                    ).arg(rosterIdentity).arg(sourceRowIndex)
                );
        }
        if (sourceRowIndex > std::numeric_limits<int>::max())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster data failed for %1: row index %2 exceeds the supported range."
                    ).arg(rosterIdentity).arg(sourceRowIndex)
                );
        }
        const int rowIndex = static_cast<int>(sourceRowIndex);

        Roster& roster = records[requestOrder].roster;
        while (roster.rows.size() <= rowIndex)
        {
            QStringList emptyRow;
            emptyRow.reserve(roster.columns.size());
            for (qsizetype column = 0;
                 column < roster.columns.size();
                 ++column)
            {
                emptyRow.append(QString());
            }
            roster.rows.append(std::move(emptyRow));
        }
        roster.rows[rowIndex][columnIndex] =
            cellsQuery.value("value").toString();
    }
    if (cellsQuery.lastError().isValid())
    {
        return std::unexpected(
            (templatePrintContext
                ? QObject::tr("Reading template print roster cells failed: %1")
                : QObject::tr("Reading full roster cells failed: %1"))
                .arg(cellsQuery.lastError().text())
            );
    }
    if (invalidCellIdentity)
    {
        return std::unexpected(templatePrintContext
            ? QObject::tr(
                "Loading template print roster cells returned an invalid class identity.")
            : QObject::tr(
                "Loading full roster cells returned an invalid class identity."));
    }

    return records;
}

Result<QList<RosterRepository::FullRosterReadRecord>>
RosterRepository::loadFullRosters(
    const QList<int>& classIds
    )
{
    return loadFullRostersImpl(classIds, nullptr, false);
}

Result<QList<RosterRepository::TemplatePrintReadRecord>>
RosterRepository::loadRostersForTemplatePrint(
    const QList<int>& classIds
    )
{
    return loadFullRostersImpl(
        classIds, &m_templatePrintBatchReadMetrics, true);
}

const RosterRepository::TemplatePrintBatchReadMetrics&
RosterRepository::templatePrintBatchReadMetrics() const noexcept
{
    return m_templatePrintBatchReadMetrics;
}

Result<QList<RosterRepository::ColumnNamesForClass>>
RosterRepository::loadRosterColumnNamesForClasses(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<ColumnNamesForClass>{};
    }

    QList<ColumnNamesForClass> records;
    records.reserve(classIds.size());
    QHash<int, qsizetype> recordIndexByClassId;
    recordIndexByClassId.reserve(classIds.size());
    QStringList requestedClasses;
    requestedClasses.reserve(classIds.size());

    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds.at(index);
        if (classId <= 0 || recordIndexByClassId.contains(classId))
        {
            return std::unexpected(
                QObject::tr("Loading roster columns failed: class ids must be positive and unique.")
                );
        }

        recordIndexByClassId.insert(classId, index);
        records.append({.classId = classId});
        requestedClasses.append(
            QStringLiteral("(%1, %2)")
                .arg(classId)
                .arg(index)
            );
    }

    const QString queryText = QStringLiteral(R"(
        WITH requested(class_id, request_order) AS (
            VALUES %1
        )
        SELECT
            requested.class_id AS class_id,
            roster_columns.name AS name
        FROM requested
        LEFT JOIN roster_columns
            ON roster_columns.class_id=requested.class_id
        ORDER BY
            requested.request_order,
            roster_columns.position,
            roster_columns.id
    )").arg(requestedClasses.join(QStringLiteral(", ")));

    QSqlQuery query(m_database);
    if (!query.prepare(queryText))
    {
        return std::unexpected(
            QObject::tr("Preparing roster column batch read failed: %1")
                .arg(query.lastError().text())
            );
    }

    const auto loadedColumns = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading roster column names"),
        QObject::tr("%1 classes").arg(classIds.size())
        );
    if (!loadedColumns)
    {
        return std::unexpected(loadedColumns.error().userMessage());
    }

    while (query.next())
    {
        if (query.value("name").isNull())
        {
            continue;
        }

        const int classId = query.value("class_id").toInt();
        const auto recordIndex = recordIndexByClassId.constFind(classId);
        if (recordIndex == recordIndexByClassId.cend())
        {
            return std::unexpected(
                QObject::tr("Loading roster column names returned an unexpected class id.")
                );
        }

        records[*recordIndex].columns.append(query.value("name").toString());
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(
            QObject::tr("Reading roster column batch failed: %1")
                .arg(query.lastError().text())
            );
    }

    return records;
}

Result<QList<RosterRepository::MyClassesStudentCountReadEntry>>
RosterRepository::loadMyClassesStudentCountRecords(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<MyClassesStudentCountReadEntry>{};
    }

    ++m_myClassesStudentCountBatchReadMetrics.callCount;
    m_myClassesStudentCountBatchReadMetrics.requestedClassCount +=
        classIds.size();

    QSet<int> seenClassIds;
    seenClassIds.reserve(classIds.size());
    QHash<int, qsizetype> requestIndexByClassId;
    requestIndexByClassId.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds.at(index);
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(QObject::tr(
                "Loading My Classes student counts failed: class ids must be positive and unique."
                ));
        }
        seenClassIds.insert(classId);
        requestIndexByClassId.insert(classId, index);
    }

    const auto readIndividually =
        [this, &classIds]()
        -> Result<QList<MyClassesStudentCountReadEntry>>
    {
        QList<MyClassesStudentCountReadEntry> entries;
        entries.reserve(classIds.size());
        for (const int classId : classIds)
        {
            ++m_myClassesStudentCountBatchReadMetrics.fallbackClassReadCount;
            entries.append({
                classId,
                loadMyClassesStudentCountRecord(classId)
            });
        }
        return entries;
    };

    ++m_myClassesStudentCountBatchReadMetrics.columnStatementCount;
    const auto loadedColumns = loadRosterColumnNamesForClasses(classIds);
    if (!loadedColumns || loadedColumns->size() != classIds.size())
    {
        return readIndividually();
    }

    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const ColumnNamesForClass& record = loadedColumns->at(index);
        if (record.classId != classIds.at(index))
        {
            return readIndividually();
        }

        const int englishColumn = record.columns.indexOf(
            QStringLiteral("English")
            );
        const int koreanColumn = record.columns.indexOf(
            QStringLiteral("Korean")
            );
        requestedValues.append(
            QStringLiteral("(%1, %2, %3, %4)")
                .arg(classIds.at(index))
                .arg(index)
                .arg(englishColumn)
                .arg(koreanColumn)
            );
    }

    const QString queryText = QStringLiteral(R"(
        WITH requested(
            class_id,
            request_order,
            english_column,
            korean_column
        ) AS (VALUES %1)
        SELECT requested.class_id AS class_id,
               requested.request_order AS request_order,
               roster_data.row_index AS row_index,
               typeof(roster_data.row_index) AS row_index_storage_type,
               roster_data.value AS value
        FROM requested
        JOIN roster_data
            ON roster_data.class_id=requested.class_id
        WHERE roster_data.row_index >= 0
            AND (
                (requested.english_column >= 0
                    AND roster_data.col_index=requested.english_column)
                OR
                (requested.korean_column >= 0
                    AND roster_data.col_index=requested.korean_column)
            )
        ORDER BY
            requested.request_order,
            roster_data.row_index,
            roster_data.col_index
    )").arg(requestedValues.join(QStringLiteral(", ")));

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    ++m_myClassesStudentCountBatchReadMetrics.dataStatementCount;
    const auto loadedCells = SqlQueryUtils::execute(
        query,
        queryText,
        QObject::tr("Loading My Classes student-count cells"),
        QObject::tr("%1 classes").arg(classIds.size())
        );
    if (!loadedCells)
    {
        query.finish();
        return readIndividually();
    }

    std::vector<int> counts(static_cast<std::size_t>(classIds.size()), 0);
    int currentRequestOrder = -1;
    qlonglong currentRowIndex = -1;
    bool currentRowHasStudent = false;
    const auto finishCurrentRow = [&]()
    {
        if (currentRowHasStudent
            && currentRequestOrder >= 0
            && currentRequestOrder < classIds.size())
        {
            ++counts[static_cast<std::size_t>(currentRequestOrder)];
        }
    };

    while (query.next())
    {
        const int classId = query.value("class_id").toInt();
        const int requestOrder = query.value("request_order").toInt();
        if (query.value("row_index_storage_type").toString()
            != QStringLiteral("integer"))
        {
            query.finish();
            return readIndividually();
        }
        bool rowIndexValid = false;
        const qlonglong rowIndex =
            query.value("row_index").toLongLong(&rowIndexValid);
        if (requestOrder < 0
            || requestOrder >= classIds.size()
            || classIds.at(requestOrder) != classId
            || !requestIndexByClassId.contains(classId)
            || !rowIndexValid
            || rowIndex < 0)
        {
            query.finish();
            return readIndividually();
        }

        if (requestOrder != currentRequestOrder
            || rowIndex != currentRowIndex)
        {
            finishCurrentRow();
            currentRequestOrder = requestOrder;
            currentRowIndex = rowIndex;
            currentRowHasStudent = false;
        }

        if (!query.value("value").toString().trimmed().isEmpty())
        {
            currentRowHasStudent = true;
        }
    }
    if (query.lastError().isValid())
    {
        query.finish();
        return readIndividually();
    }
    finishCurrentRow();

    QList<MyClassesStudentCountReadEntry> entries;
    entries.reserve(classIds.size());
    for (std::size_t index = 0; index < counts.size(); ++index)
    {
        entries.append({
            classIds.at(static_cast<qsizetype>(index)),
            Result<int>{counts[index]}
        });
    }
    return entries;
}

const RosterRepository::MyClassesStudentCountBatchReadMetrics&
RosterRepository::myClassesStudentCountBatchReadMetrics() const noexcept
{
    return m_myClassesStudentCountBatchReadMetrics;
}

Result<QList<RosterRepository::SubPrepStudentCountReadEntry>>
RosterRepository::loadSubPrepStudentCountRecords(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<SubPrepStudentCountReadEntry>{};
    }

    ++m_subPrepStudentCountBatchReadMetrics.callCount;
    m_subPrepStudentCountBatchReadMetrics.requestedClassCount +=
        classIds.size();

    QSet<int> seenClassIds;
    seenClassIds.reserve(classIds.size());
    QHash<int, qsizetype> requestIndexByClassId;
    requestIndexByClassId.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds.at(index);
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep student counts failed: class ids must be positive and unique."
                ));
        }
        seenClassIds.insert(classId);
        requestIndexByClassId.insert(classId, index);
    }

    const auto readIndividually =
        [this, &classIds]()
        -> Result<QList<SubPrepStudentCountReadEntry>>
    {
        QList<SubPrepStudentCountReadEntry> entries;
        entries.reserve(classIds.size());
        for (const int classId : classIds)
        {
            ++m_subPrepStudentCountBatchReadMetrics.fallbackClassReadCount;
            const auto loaded = loadSubPrepStudentCountRecord(classId);
            if (loaded)
            {
                entries.append(loaded.value());
            }
            else
            {
                entries.append({
                    classId,
                    Result<int>{std::unexpected(loaded.error())},
                    0,
                    0,
                    0
                });
            }
        }
        return entries;
    };

    ++m_subPrepStudentCountBatchReadMetrics.columnStatementCount;
    const auto loadedColumns = loadRosterColumnNamesForClasses(classIds);
    if (!loadedColumns || loadedColumns->size() != classIds.size())
    {
        return readIndividually();
    }

    QList<SubPrepStudentCountReadEntry> entries;
    entries.reserve(classIds.size());
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    std::vector<int> englishColumns(
        static_cast<std::size_t>(classIds.size()),
        -1
        );
    std::vector<int> koreanColumns(
        static_cast<std::size_t>(classIds.size()),
        -1
        );
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const ColumnNamesForClass& record = loadedColumns->at(index);
        if (record.classId != classIds.at(index))
        {
            return readIndividually();
        }

        const int englishColumn = record.columns.indexOf(
            QStringLiteral("English")
            );
        const int koreanColumn = record.columns.indexOf(
            QStringLiteral("Korean")
            );
        const qsizetype rawColumnCount = record.columns.size();
        const int columnCount =
            rawColumnCount > std::numeric_limits<int>::max()
                ? std::numeric_limits<int>::max()
                : static_cast<int>(rawColumnCount);
        englishColumns[static_cast<std::size_t>(index)] = englishColumn;
        koreanColumns[static_cast<std::size_t>(index)] = koreanColumn;
        entries.append({
            .classId = record.classId,
            .studentCount = 0,
            .columnCount = columnCount,
            .rowCount = 0,
            .cellCount = 0
        });

        // loadRoster() returns immediately after the column query when there
        // are no columns. Preserve that success case without touching
        // roster_data.
        if (!record.columns.isEmpty())
        {
            requestedValues.append(
                QStringLiteral("(%1, %2, %3)")
                    .arg(record.classId)
                    .arg(index)
                    .arg(record.columns.size())
                );
        }
    }

    if (requestedValues.isEmpty())
    {
        return entries;
    }

    const QString queryText = QStringLiteral(R"(
        WITH requested(class_id, request_order, column_count) AS (
            VALUES %1
        )
        SELECT requested.class_id AS class_id,
               requested.request_order AS request_order,
               roster_data.row_index AS row_index,
               typeof(roster_data.row_index) AS row_index_storage_type,
               roster_data.col_index AS col_index,
               typeof(roster_data.col_index) AS col_index_storage_type,
               roster_data.value AS value
        FROM requested
        JOIN roster_data
            ON roster_data.class_id=requested.class_id
        WHERE roster_data.row_index >= 0
            AND roster_data.col_index >= 0
            AND roster_data.col_index < requested.column_count
        ORDER BY
            requested.request_order,
            roster_data.row_index,
            roster_data.col_index
    )").arg(requestedValues.join(QStringLiteral(", ")));

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    ++m_subPrepStudentCountBatchReadMetrics.dataStatementCount;
    const auto loadedCells = SqlQueryUtils::execute(
        query,
        queryText,
        QObject::tr("Loading Sub Prep student-count cells"),
        QObject::tr("%1 classes").arg(requestedValues.size())
        );
    if (!loadedCells)
    {
        query.finish();
        return readIndividually();
    }

    std::vector<int> rowCounts(static_cast<std::size_t>(classIds.size()), 0);
    std::vector<int> studentCounts(
        static_cast<std::size_t>(classIds.size()),
        0
        );
    int currentRequestOrder = -1;
    qlonglong currentRowIndex = -1;
    bool currentRowHasStudent = false;
    const auto finishCurrentRow = [&]()
    {
        if (currentRowHasStudent
            && currentRequestOrder >= 0
            && currentRequestOrder < classIds.size())
        {
            ++studentCounts[static_cast<std::size_t>(currentRequestOrder)];
        }
    };

    while (query.next())
    {
        const int classId = query.value("class_id").toInt();
        const int requestOrder = query.value("request_order").toInt();
        if (query.value("row_index_storage_type").toString()
            != QStringLiteral("integer"))
        {
            query.finish();
            return readIndividually();
        }
        bool rowIndexValid = false;
        const qlonglong rowIndex =
            query.value("row_index").toLongLong(&rowIndexValid);
        if (query.value("col_index_storage_type").toString()
            != QStringLiteral("integer"))
        {
            query.finish();
            return readIndividually();
        }
        const int columnIndex = query.value("col_index").toInt();
        if (requestOrder < 0
            || requestOrder >= classIds.size()
            || classIds.at(requestOrder) != classId
            || !requestIndexByClassId.contains(classId)
            || entries.at(requestOrder).classId != classId
            || !rowIndexValid
            || rowIndex < 0)
        {
            query.finish();
            return readIndividually();
        }

        const int rowExtent =
            rowIndex >= std::numeric_limits<int>::max()
                ? std::numeric_limits<int>::max()
                : static_cast<int>(rowIndex) + 1;
        int& rowCount = rowCounts[static_cast<std::size_t>(requestOrder)];
        rowCount = std::max(rowCount, rowExtent);

        if (requestOrder != currentRequestOrder
            || rowIndex != currentRowIndex)
        {
            finishCurrentRow();
            currentRequestOrder = requestOrder;
            currentRowIndex = rowIndex;
            currentRowHasStudent = false;
        }

        const std::size_t requestOffset =
            static_cast<std::size_t>(requestOrder);
        if (columnIndex == englishColumns[requestOffset]
            || columnIndex == koreanColumns[requestOffset])
        {
            if (!query.value("value").toString().trimmed().isEmpty())
            {
                currentRowHasStudent = true;
            }
        }
    }
    if (query.lastError().isValid())
    {
        query.finish();
        return readIndividually();
    }
    finishCurrentRow();

    for (qsizetype index = 0; index < entries.size(); ++index)
    {
        const std::size_t offset = static_cast<std::size_t>(index);
        entries[index].studentCount = studentCounts[offset];
        entries[index].rowCount = rowCounts[offset];
        const int rowCount = entries[index].rowCount;
        const int columnCount = entries[index].columnCount;
        if (rowCount == 0 || columnCount == 0)
        {
            entries[index].cellCount = 0;
        }
        else if (
            rowCount > std::numeric_limits<int>::max() / columnCount
            )
        {
            entries[index].cellCount = std::numeric_limits<int>::max();
        }
        else
        {
            entries[index].cellCount = rowCount * columnCount;
        }
    }
    return entries;
}

const RosterRepository::SubPrepStudentCountBatchReadMetrics&
RosterRepository::subPrepStudentCountBatchReadMetrics() const noexcept
{
    return m_subPrepStudentCountBatchReadMetrics;
}

Status RosterRepository::forEachRosterDataCellForClasses(
    const QList<int>& classIds,
    const int rowLimit,
    const std::function<void(int, int, int, const QString&)>& consumer
    )
{
    if (classIds.isEmpty())
    {
        return {};
    }
    if (rowLimit <= 0 || !consumer)
    {
        return std::unexpected(
            QObject::tr("Loading roster availability failed: invalid request.")
            );
    }

    QStringList requestedClasses;
    requestedClasses.reserve(classIds.size());
    QSet<int> seenClassIds;
    seenClassIds.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds.at(index);
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(
                QObject::tr("Loading roster availability failed: class ids must be positive and unique.")
                );
        }

        seenClassIds.insert(classId);
        requestedClasses.append(
            QStringLiteral("(%1, %2)")
                .arg(classId)
                .arg(index)
            );
    }

    const QString queryText = QStringLiteral(R"(
        WITH requested(class_id, request_order) AS (
            VALUES %1
        )
        SELECT
            requested.class_id AS class_id,
            roster_data.row_index AS row_index,
            typeof(roster_data.row_index) AS row_index_storage_type,
            roster_data.col_index AS col_index,
            typeof(roster_data.col_index) AS col_index_storage_type,
            roster_data.value AS value
        FROM requested
        JOIN roster_data
            ON roster_data.class_id=requested.class_id
        WHERE roster_data.row_index >= 0
            AND roster_data.row_index < ?
        ORDER BY
            requested.request_order,
            roster_data.row_index,
            roster_data.col_index
    )").arg(requestedClasses.join(QStringLiteral(", ")));

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    if (!query.prepare(queryText))
    {
        return std::unexpected(
            QObject::tr("Preparing roster availability read failed: %1")
                .arg(query.lastError().text())
            );
    }
    query.addBindValue(rowLimit);

    const auto loadedCells = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading roster availability cells"),
        QObject::tr("%1 classes").arg(classIds.size())
        );
    if (!loadedCells)
    {
        return std::unexpected(loadedCells.error().userMessage());
    }

    while (query.next())
    {
        if (
            query.value("row_index_storage_type").toString()
                != QStringLiteral("integer")
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster availability failed: invalid row index."
                    )
                );
        }

        if (
            query.value("col_index_storage_type").toString()
                != QStringLiteral("integer")
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster availability failed: invalid column index."
                    )
                );
        }

        bool columnIndexValid = false;
        const qlonglong columnIndex =
            query.value("col_index").toLongLong(&columnIndexValid);
        if (!columnIndexValid)
        {
            return std::unexpected(
                QObject::tr(
                    "Loading roster availability failed: invalid column index."
                    )
                );
        }
        if (
            columnIndex < std::numeric_limits<int>::min()
            || columnIndex > std::numeric_limits<int>::max()
            )
        {
            continue;
        }

        consumer(
            query.value("class_id").toInt(),
            query.value("row_index").toInt(),
            static_cast<int>(columnIndex),
            query.value("value").toString()
            );
    }

    return {};
}

Result<int> RosterRepository::loadMyClassesStudentCountRecord(
    const int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading My Classes student count failed: invalid class id %1.")
                .arg(classId)
            );
    }

    ++m_myClassesStudentCountBatchReadMetrics.columnStatementCount;
    const auto loadedColumns = loadRosterColumnNamesForClasses({classId});
    if (!loadedColumns)
    {
        return std::unexpected(loadedColumns.error());
    }
    if (loadedColumns->size() != 1 || loadedColumns->front().classId != classId)
    {
        return std::unexpected(QObject::tr(
            "Loading My Classes student count failed: roster columns returned an incomplete class record."
            ));
    }

    const QStringList& columns = loadedColumns->front().columns;
    const int englishColumn = columns.indexOf(QStringLiteral("English"));
    const int koreanColumn = columns.indexOf(QStringLiteral("Korean"));
    if (englishColumn < 0 && koreanColumn < 0)
    {
        return 0;
    }

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    if (!query.prepare(R"(
        SELECT row_index,
               typeof(row_index) AS row_index_storage_type,
               value
        FROM roster_data
        WHERE class_id=?
            AND row_index >= 0
            AND (
                (? >= 0 AND col_index=?)
                OR
                (? >= 0 AND col_index=?)
            )
        ORDER BY row_index, col_index
    )"))
    {
        return std::unexpected(
            QObject::tr("Preparing My Classes student-count read failed: %1")
                .arg(query.lastError().text())
            );
    }
    query.addBindValue(classId);
    query.addBindValue(englishColumn);
    query.addBindValue(englishColumn);
    query.addBindValue(koreanColumn);
    query.addBindValue(koreanColumn);

    ++m_myClassesStudentCountBatchReadMetrics.dataStatementCount;
    const auto loadedCells = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading My Classes student-count cells"),
        QObject::tr("class id %1").arg(classId)
        );
    if (!loadedCells)
    {
        return std::unexpected(loadedCells.error().userMessage());
    }

    int count = 0;
    qlonglong currentRowIndex = -1;
    bool currentRowHasStudent = false;
    const auto finishCurrentRow = [&]()
    {
        if (currentRowIndex >= 0 && currentRowHasStudent)
        {
            ++count;
        }
    };
    while (query.next())
    {
        if (query.value("row_index_storage_type").toString()
            != QStringLiteral("integer"))
        {
            return std::unexpected(
                QObject::tr(
                    "Reading My Classes student-count cells failed for class id %1: invalid row index."
                    ).arg(classId)
                );
        }
        bool rowIndexValid = false;
        const qlonglong rowIndex =
            query.value("row_index").toLongLong(&rowIndexValid);
        if (!rowIndexValid)
        {
            return std::unexpected(
                QObject::tr(
                    "Reading My Classes student-count cells failed for class id %1: invalid row index."
                    ).arg(classId)
                );
        }
        if (rowIndex < 0)
        {
            continue;
        }
        if (rowIndex != currentRowIndex)
        {
            finishCurrentRow();
            currentRowIndex = rowIndex;
            currentRowHasStudent = false;
        }
        if (!query.value("value").toString().trimmed().isEmpty())
        {
            currentRowHasStudent = true;
        }
    }
    if (query.lastError().isValid())
    {
        return std::unexpected(
            QObject::tr("Reading My Classes student-count cells failed: %1")
                .arg(query.lastError().text())
            );
    }
    finishCurrentRow();
    return count;
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
            "SELECT MAX(row_index) AS maximum_row_index, "
            "MAX(CASE WHEN typeof(row_index) <> 'integer' THEN 1 ELSE 0 END) "
            "AS has_non_integer_row_index FROM roster_data "
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

    const QVariant maximumRowValue = maximumRowQuery.value(
        QStringLiteral("maximum_row_index")
        );
    std::size_t rowCount = 0;
    if (!maximumRowValue.isNull())
    {
        bool storageFlagValid = false;
        const int hasNonIntegerRowIndex = maximumRowQuery.value(
            QStringLiteral("has_non_integer_row_index")
            ).toInt(&storageFlagValid);
        if (!storageFlagValid || hasNonIntegerRowIndex != 0)
        {
            return std::unexpected(
                QObject::tr(
                    "Roster row_index has non-INTEGER SQLite storage."
                    )
                );
        }

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

Result<RosterRepository::SubPrepStudentCountReadEntry>
RosterRepository::loadSubPrepStudentCountRecord(
    const int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading Sub Prep student count failed: invalid class id %1.")
                .arg(classId)
            );
    }

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

    int count = 0;
    if (englishColumn >= 0 || koreanColumn >= 0)
    {
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
    }

    return SubPrepStudentCountReadEntry{
        .classId = classId,
        .studentCount = count,
        .columnCount = static_cast<int>(roster->columns.size()),
        .rowCount = static_cast<int>(roster->rows.size()),
        .cellCount = cellCount
    };
}

Result<int> RosterRepository::getRosterStudentCount(
    const int classId
    )
{
    const auto loaded = loadSubPrepStudentCountRecord(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    StartupProfiler::recordSubPrepRosterQuery(
        classId,
        loaded->columnCount,
        loaded->rowCount,
        loaded->cellCount,
        loaded->studentCount.value()
        );

    return loaded->studentCount.value();
}

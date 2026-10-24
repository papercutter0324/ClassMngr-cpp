#include "speaking_eval_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "next/domain/speaking_evaluation_grade.h"

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>

#include <array>
#include <string_view>
#include <utility>

namespace NextDomain = ClassMngr::Next::Domain;

SpeakingEvalRepository::SpeakingEvalRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Status SpeakingEvalRepository::forEachClassAnalyticsEvaluationRow(
    const int classId,
    const std::array<QString, 4>& evaluationNames,
    const std::function<void(const ClassAnalyticsEvaluationRow&)>& consumer
    )
{
    QSqlQuery rowsQuery(m_database);
    rowsQuery.setForwardOnly(true);
    rowsQuery.prepare(R"(
        WITH requested(evaluation_name, request_order) AS (
            VALUES (?, 0), (?, 1), (?, 2), (?, 3)
        )
        SELECT requested.request_order AS request_order,
               data.evaluation_id AS data_evaluation_id,
               data.col_1 AS col_1,
               data.col_2 AS col_2,
               data.col_3 AS col_3,
               data.col_4 AS col_4,
               data.col_5 AS col_5,
               data.col_6 AS col_6,
               data.col_7 AS col_7,
               data.col_8 AS col_8
        FROM requested
        LEFT JOIN speaking_evaluations evaluation
            ON evaluation.class_id=?
           AND evaluation.evaluation_name=requested.evaluation_name
        LEFT JOIN speaking_eval_data data
            ON data.evaluation_id=evaluation.id
        ORDER BY requested.request_order, data.row_index
    )");
    for (const QString& name : evaluationNames)
    {
        rowsQuery.addBindValue(name);
    }
    rowsQuery.addBindValue(classId);
    const auto executed = SqlQueryUtils::executePrepared(
        rowsQuery,
        QStringLiteral("Loading Class Analytics evaluations"),
        QStringLiteral("class id %1").arg(classId));
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    std::array<bool, 4> seenRequests{};
    while (rowsQuery.next())
    {
        bool requestOrderIsValid = false;
        const int requestOrder = rowsQuery.value("request_order")
            .toInt(&requestOrderIsValid);
        if (!requestOrderIsValid
            || requestOrder < 0
            || requestOrder >= static_cast<int>(evaluationNames.size()))
        {
            return std::unexpected(QStringLiteral(
                "Class Analytics returned an invalid evaluation order."
                ));
        }
        seenRequests[static_cast<std::size_t>(requestOrder)] = true;

        // LEFT JOIN contributes one placeholder row for a missing
        // evaluation or an evaluation with no stored score rows.
        if (rowsQuery.value("data_evaluation_id").isNull())
        {
            continue;
        }

        ClassAnalyticsEvaluationRow row;
        row.evaluationIndex = static_cast<std::size_t>(requestOrder);
        row.englishName = rowsQuery.value("col_1")
            .toString();
        row.koreanName = rowsQuery.value("col_2")
            .toString();
        for (std::size_t score = 0; score < row.scores.size(); ++score)
        {
            row.scores[score] = rowsQuery.value(
                QStringLiteral("col_%1").arg(score + 3))
                .toString();
        }
        consumer(row);
    }
    if (rowsQuery.lastError().isValid())
    {
        return std::unexpected(rowsQuery.lastError().text());
    }
    for (const bool seen : seenRequests)
    {
        if (!seen)
        {
            return std::unexpected(QStringLiteral(
                "Class Analytics returned an incomplete evaluation batch."
                ));
        }
    }
    return {};
}

Status SpeakingEvalRepository::saveSpeakingEval(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    )
{
    const SaveResult saved = saveSpeakingEvalInternal(
        classId,
        evaluationName,
        rows,
        dirtyCells,
        ParentClassPolicy::PreserveLegacyForeignKey
        );
    if (!saved)
    {
        return std::unexpected(saved.error().diagnostic);
    }

    return {};
}

SpeakingEvalRepository::SaveResult
SpeakingEvalRepository::saveSpeakingEvalDetailed(
    const int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    )
{
    return saveSpeakingEvalInternal(
        classId,
        evaluationName,
        rows,
        dirtyCells,
        ParentClassPolicy::ClassifyMissingParent
        );
}

SpeakingEvalRepository::SaveResult
SpeakingEvalRepository::saveSpeakingEvalInternal(
    const int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells,
    const ParentClassPolicy parentClassPolicy
    )
{
    const auto technicalFailure = [](QString diagnostic) -> SaveResult
    {
        return std::unexpected(SaveFailure{
            .kind = SaveFailure::Kind::Technical,
            .diagnostic = std::move(diagnostic)
        });
    };

    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return technicalFailure(
            QObject::tr(
                "Saving speaking evaluation failed: invalid class id or "
                "evaluation name."
                )
            );
    }

    const QString normalizedEvaluationName = evaluationName.trimmed();
    const QString identity = QObject::tr("evaluation '%1' for class id %2")
        .arg(normalizedEvaluationName)
        .arg(classId);
    const bool classifyMissingParent =
        parentClassPolicy == ParentClassPolicy::ClassifyMissingParent;
    DatabaseTransaction transaction(
        m_database,
        classifyMissingParent
            ? DatabaseTransaction::Mode::Immediate
            : DatabaseTransaction::Mode::Default
        );
    if (!transaction.started())
    {
        return technicalFailure(
            QObject::tr("Starting speaking evaluation transaction failed for %1: %2")
                .arg(
                    identity,
                    classifyMissingParent
                        ? transaction.startError()
                        : m_database.lastError().text()
                    )
            );
    }

    if (classifyMissingParent)
    {
        QSqlQuery parentQuery(m_database);
        parentQuery.prepare(QStringLiteral(
            "SELECT 1 FROM classes WHERE id=?"
            ));
        parentQuery.addBindValue(classId);

        const auto loaded = SqlQueryUtils::executePrepared(
            parentQuery,
            QObject::tr("Checking speaking evaluation parent class"),
            QObject::tr("class id %1").arg(classId)
            );
        if (!loaded)
        {
            return technicalFailure(loaded.error().userMessage());
        }

        const bool parentClassExists = parentQuery.next();
        const QSqlError fetchError = parentQuery.lastError();
        parentQuery.finish();
        if (fetchError.isValid())
        {
            return technicalFailure(
                QObject::tr(
                    "Checking speaking evaluation parent class failed for "
                    "class id %1: %2"
                    ).arg(classId).arg(fetchError.text())
                );
        }

        if (!parentClassExists)
        {
            if (!transaction.rollbackChecked())
            {
                return technicalFailure(
                    QObject::tr(
                        "Rolling back speaking evaluation save failed for "
                        "class id %1: %2"
                        ).arg(classId).arg(m_database.lastError().text())
                    );
            }

            return std::unexpected(SaveFailure{
                .kind = SaveFailure::Kind::ParentClassMissing,
                .diagnostic = QObject::tr(
                    "Saving speaking evaluation failed: parent class id %1 "
                    "was not found."
                    ).arg(classId)
            });
        }
    }

    QSqlQuery query(m_database);
    auto execute = [&](const QString& action) -> Status
    {
        const auto result = SqlQueryUtils::executePrepared(
            query, action, identity);
        return result
            ? Status{}
            : Status(std::unexpected(result.error().userMessage()));
    };

    qint64 evaluationId = -1;

    query.prepare(R"(
        SELECT id
        FROM speaking_evaluations
        WHERE class_id=? AND evaluation_name=?
    )");

    query.addBindValue(classId);
    query.addBindValue(normalizedEvaluationName);

    Status statement = execute(QObject::tr("Loading speaking evaluation"));
    if (!statement)
    {
        return technicalFailure(statement.error());
    }

    const bool evaluationExists = query.next();
    const QSqlError evaluationFetchError = query.lastError();
    if (evaluationFetchError.isValid())
    {
        return technicalFailure(
            QObject::tr(
                "Loading speaking evaluation failed for %1: %2"
                ).arg(identity, evaluationFetchError.text())
            );
    }

    if (evaluationExists)
    {
        evaluationId =
            query.value("id").toLongLong();
    }
    else
    {
        query.prepare(R"(
            INSERT INTO speaking_evaluations (
                class_id,
                evaluation_name
            )
            VALUES (?, ?)
        )");

        query.addBindValue(classId);
        query.addBindValue(normalizedEvaluationName);

        statement = execute(QObject::tr("Creating speaking evaluation"));
        if (!statement)
        {
            return technicalFailure(statement.error());
        }

        evaluationId =
            query.lastInsertId().toLongLong();

        if (evaluationId <= 0)
        {
            return technicalFailure(
                QObject::tr(
                    "Creating %1 failed: the database did not return a valid "
                    "record id."
                    ).arg(identity)
                );
        }
    }

    for (int row = 0; row < SpeakingEval::RowCount; ++row)
    {
        query.prepare(R"(
            INSERT OR IGNORE INTO speaking_eval_data (
                evaluation_id,
                row_index
            )
            VALUES (?, ?)
        )");

        query.addBindValue(evaluationId);
        query.addBindValue(row);

        statement = execute(QObject::tr("Ensuring speaking evaluation row"));
        if (!statement)
        {
            return technicalFailure(statement.error());
        }
    }

    query.prepare(R"(
        SELECT *, typeof(row_index) AS row_index_storage_type
        FROM speaking_eval_data
        WHERE evaluation_id=?
    )");

    query.addBindValue(evaluationId);

    statement = execute(QObject::tr("Loading speaking evaluation rows"));
    if (!statement)
    {
        return technicalFailure(statement.error());
    }

    QHash<int, QStringList> existingRows;

    while (query.next())
    {
        QStringList values;

        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            values.append(
                query.value(
                    QStringLiteral("col_%1")
                        .arg(column)
                    ).toString()
                );
        }

        const QVariant rowIndexValue = query.value("row_index");
        const QString rowIndexStorageType =
            query.value("row_index_storage_type").toString();
        if (rowIndexStorageType == QStringLiteral("text"))
        {
            bool rowIndexValid = false;
            rowIndexValue.toLongLong(&rowIndexValid);
            if (!rowIndexValid)
            {
                return technicalFailure(
                    QObject::tr(
                        "Loading speaking evaluation rows failed for %1: "
                        "invalid row index."
                        ).arg(identity)
                    );
            }
        }
        if (rowIndexStorageType != QStringLiteral("integer"))
        {
            return technicalFailure(
                QObject::tr(
                    "Loading speaking evaluation rows failed for %1: "
                    "invalid row index."
                    ).arg(identity)
                );
        }

        existingRows.insert(
            rowIndexValue.toInt(),
            values
            );
    }

    const QSqlError rowsFetchError = query.lastError();
    if (rowsFetchError.isValid())
    {
        return technicalFailure(
            QObject::tr(
                "Loading speaking evaluation rows failed for %1: %2"
                ).arg(identity, rowsFetchError.text())
            );
    }

    QList<SpeakingEvalCellChange> cellsToUpdate =
        dirtyCells;

    if (cellsToUpdate.isEmpty())
    {
        for (int row = 0; row < SpeakingEval::RowCount; ++row)
        {
            for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
            {
                cellsToUpdate.append({ row, column });
            }
        }
    }

    for (const SpeakingEvalCellChange& cell : cellsToUpdate)
    {
        if (
            cell.row < 0
            || cell.row >= SpeakingEval::RowCount
            || cell.column < 0
            || cell.column >= SpeakingEval::ColumnCount
            )
        {
            continue;
        }

        const QString newValue =
            cell.row < rows.size()
            && cell.column < rows[cell.row].size()
                ? rows[cell.row][cell.column]
                : QString();

        const QStringList existingRow =
            existingRows.value(cell.row);

        const QString oldValue =
            cell.column < existingRow.size()
                ? existingRow[cell.column]
                : QString();

        if ((oldValue.isNull() ? QString() : oldValue) == newValue)
        {
            continue;
        }

        query.prepare(
            QString(R"(
                UPDATE speaking_eval_data
                SET col_%1=?
                WHERE evaluation_id=? AND row_index=?
            )").arg(cell.column)
            );

        query.addBindValue(newValue);
        query.addBindValue(evaluationId);
        query.addBindValue(cell.row);

        statement = execute(QObject::tr("Updating speaking evaluation cell"));
        if (!statement)
        {
            return technicalFailure(statement.error());
        }
    }

    if (!transaction.commit())
    {
        return technicalFailure(
            QObject::tr("Committing speaking evaluation failed for %1: %2")
                .arg(identity, m_database.lastError().text())
            );
    }

    return {};
}

Result<SpeakingEvalRows> SpeakingEvalRepository::loadSpeakingEval(
    int classId,
    const QString& evaluationName
    )
{
    SpeakingEvalRows rows;

    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return std::unexpected(
            QObject::tr(
                "Loading speaking evaluation failed: invalid class id or "
                "evaluation name."
                )
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT id
        FROM speaking_evaluations
        WHERE class_id=? AND evaluation_name=?
    )");

    query.addBindValue(classId);
    query.addBindValue(evaluationName);

    const QString identity = QObject::tr(
        "class id %1, evaluation '%2'")
        .arg(classId)
        .arg(evaluationName);
    const auto loadedEvaluation = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading speaking evaluation"),
        identity
        );
    if (!loadedEvaluation)
    {
        return std::unexpected(loadedEvaluation.error().userMessage());
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return std::unexpected(
                SqlQueryUtils::errorFor(
                    query,
                    QObject::tr("Loading speaking evaluation"),
                    {},
                    identity
                    ).userMessage()
                );
        }

        return rows;
    }

    const qint64 evaluationId =
        query.value("id").toLongLong();

    query.prepare(R"(
        SELECT *, typeof(row_index) AS row_index_storage_type
        FROM speaking_eval_data
        WHERE evaluation_id=?
        ORDER BY row_index
    )");

    query.addBindValue(evaluationId);

    const auto loadedRows = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading speaking evaluation rows"),
        identity
        );
    if (!loadedRows)
    {
        return std::unexpected(loadedRows.error().userMessage());
    }

    while (query.next())
    {
        if (
            !query.value("row_index").isNull()
            && query.value("row_index_storage_type").toString()
                != QStringLiteral("integer")
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading speaking evaluation rows failed for %1: "
                    "invalid row index storage type."
                    ).arg(identity)
                );
        }

        QStringList row;

        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            row.append(
                query.value(
                    QStringLiteral("col_%1")
                        .arg(column)
                    ).toString()
                );
        }

        rows.append(row);
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading speaking evaluation rows"),
                {},
                identity
                ).userMessage()
            );
    }

    return rows;
}

SpeakingEvalRepository::ReadResult
SpeakingEvalRepository::loadSpeakingEvalDetailed(
    const int classId,
    const QString& evaluationName
    )
{
    const auto technicalFailure = [](QString diagnostic) -> ReadResult
    {
        return std::unexpected(ReadFailure{
            .kind = ReadFailure::Kind::Technical,
            .diagnostic = std::move(diagnostic)
        });
    };

    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return technicalFailure(
            QObject::tr(
                "Loading speaking evaluation failed: invalid class id or "
                "evaluation name."
                )
            );
    }

    const QString identity = QObject::tr(
        "class id %1, evaluation '%2'")
        .arg(classId)
        .arg(evaluationName);
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT
            classes.id AS parent_class_id,
            speaking_eval_data.evaluation_id AS data_evaluation_id,
            speaking_eval_data.row_index,
            typeof(speaking_eval_data.row_index)
                AS row_index_storage_type,
            speaking_eval_data.col_0,
            speaking_eval_data.col_1,
            speaking_eval_data.col_2,
            speaking_eval_data.col_3,
            speaking_eval_data.col_4,
            speaking_eval_data.col_5,
            speaking_eval_data.col_6,
            speaking_eval_data.col_7,
            speaking_eval_data.col_8,
            speaking_eval_data.col_9,
            speaking_eval_data.col_10
        FROM classes
        LEFT JOIN speaking_evaluations
            ON speaking_evaluations.class_id=classes.id
            AND speaking_evaluations.evaluation_name=?
        LEFT JOIN speaking_eval_data
            ON speaking_eval_data.evaluation_id=speaking_evaluations.id
        WHERE classes.id=?
        ORDER BY speaking_eval_data.row_index
    )");
    query.addBindValue(evaluationName);
    query.addBindValue(classId);

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading speaking evaluation"),
        identity
        );
    if (!loaded)
    {
        return technicalFailure(loaded.error().userMessage());
    }

    SpeakingEvalRows rows;
    bool parentClassFound = false;
    while (query.next())
    {
        if (query.value("parent_class_id").isNull())
        {
            return technicalFailure(
                QObject::tr(
                    "Loading speaking evaluation failed for %1: invalid "
                    "parent class result."
                    ).arg(identity)
                );
        }

        parentClassFound = true;

        // evaluation_id is a non-null identity for every matching data row;
        // row_index may legitimately be SQL NULL in supported legacy data.
        if (query.value("data_evaluation_id").isNull())
        {
            continue;
        }

        if (
            !query.value("row_index").isNull()
            && query.value("row_index_storage_type").toString()
                != QStringLiteral("integer")
            )
        {
            return technicalFailure(
                QObject::tr(
                    "Loading speaking evaluation rows failed for %1: "
                    "invalid row index storage type."
                    ).arg(identity)
                );
        }

        QStringList row;
        row.reserve(SpeakingEval::ColumnCount);
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            row.append(
                query.value(QStringLiteral("col_%1").arg(column)).toString()
                );
        }
        rows.append(std::move(row));
    }

    if (query.lastError().isValid())
    {
        return technicalFailure(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading speaking evaluation"),
                {},
                identity
                ).userMessage()
            );
    }

    if (!parentClassFound)
    {
        return std::unexpected(ReadFailure{
            .kind = ReadFailure::Kind::ParentClassMissing,
            .diagnostic = QObject::tr(
                "Loading speaking evaluation failed: parent class id %1 "
                "was not found."
                ).arg(classId)
        });
    }

    return rows;
}

Result<QList<SpeakingEvalNamedRows>>
SpeakingEvalRepository::loadSpeakingEvalBatch(
    const int classId,
    const QStringList& evaluationNames
    )
{
    QList<SpeakingEvalNamedRows> evaluations;
    evaluations.reserve(evaluationNames.size());
    for (const QString& evaluationName : evaluationNames)
    {
        evaluations.append({
            .evaluationName = evaluationName,
            .rows = {}
        });
    }

    if (evaluationNames.isEmpty())
    {
        return evaluations;
    }

    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Loading speaking evaluations failed: invalid class id or "
                "evaluation name."
                )
            );
    }

    for (const QString& evaluationName : evaluationNames)
    {
        if (evaluationName.trimmed().isEmpty())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading speaking evaluations failed: invalid class id "
                    "or evaluation name."
                    )
                );
        }
    }

    QStringList requestedValues;
    requestedValues.reserve(evaluationNames.size());
    for (qsizetype index = 0; index < evaluationNames.size(); ++index)
    {
        requestedValues.append(QStringLiteral("(?, ?)"));
    }

    const QString queryText = QStringLiteral(R"(
        WITH requested(ordinal, evaluation_name) AS (
            VALUES %1
        )
        SELECT
            requested.ordinal,
            requested.evaluation_name,
            speaking_evaluations.id,
            speaking_eval_data.row_index,
            typeof(speaking_eval_data.row_index)
                AS row_index_storage_type,
            speaking_eval_data.col_0,
            speaking_eval_data.col_1,
            speaking_eval_data.col_2,
            speaking_eval_data.col_3,
            speaking_eval_data.col_4,
            speaking_eval_data.col_5,
            speaking_eval_data.col_6,
            speaking_eval_data.col_7,
            speaking_eval_data.col_8,
            speaking_eval_data.col_9,
            speaking_eval_data.col_10
        FROM requested
        LEFT JOIN speaking_evaluations
            ON speaking_evaluations.class_id=?
            AND speaking_evaluations.evaluation_name=requested.evaluation_name
        LEFT JOIN speaking_eval_data
            ON speaking_eval_data.evaluation_id=speaking_evaluations.id
        ORDER BY requested.ordinal, speaking_eval_data.row_index
    )").arg(requestedValues.join(QStringLiteral(", ")));

    QSqlQuery query(m_database);
    query.prepare(queryText);
    for (qsizetype index = 0; index < evaluationNames.size(); ++index)
    {
        query.addBindValue(index);
        query.addBindValue(evaluationNames.at(index));
    }
    query.addBindValue(classId);

    const QString identity = QObject::tr(
        "class id %1, %2 requested speaking evaluations"
        )
        .arg(classId)
        .arg(evaluationNames.size());
    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading speaking evaluations"),
        identity
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    while (query.next())
    {
        bool ordinalValid = false;
        const int ordinal = query.value(0).toInt(&ordinalValid);
        if (!ordinalValid || ordinal < 0 || ordinal >= evaluations.size())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading speaking evaluations failed: the database "
                    "returned an invalid requested evaluation position."
                    )
                );
        }

        // LEFT JOIN preserves requested evaluations with no matching record
        // or no stored rows as successful empty results.
        if (query.value(3).isNull())
        {
            continue;
        }

        if (query.value(4).toString() != QStringLiteral("integer"))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading speaking evaluations failed for %1: "
                    "invalid row index storage type."
                    ).arg(identity)
                );
        }

        QStringList row;
        row.reserve(SpeakingEval::ColumnCount);
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            row.append(query.value(5 + column).toString());
        }
        evaluations[ordinal].rows.append(std::move(row));
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading speaking evaluations"),
                queryText,
                identity
                ).userMessage()
            );
    }

    return evaluations;
}

SpeakingEvalRepository::BatchReadResult
SpeakingEvalRepository::loadSpeakingEvalBatchDetailed(
    const int classId,
    const QStringList& evaluationNames
    )
{
    const auto technicalFailure = [](QString diagnostic) -> BatchReadResult
    {
        return std::unexpected(ReadFailure{
            .kind = ReadFailure::Kind::Technical,
            .diagnostic = std::move(diagnostic)
        });
    };

    QList<SpeakingEvalNamedRows> evaluations;
    evaluations.reserve(evaluationNames.size());
    for (const QString& evaluationName : evaluationNames)
    {
        evaluations.append({
            .evaluationName = evaluationName,
            .rows = {}
        });
    }

    if (evaluationNames.isEmpty())
    {
        return evaluations;
    }

    if (classId <= 0)
    {
        return technicalFailure(
            QObject::tr(
                "Loading speaking evaluations failed: invalid class id or "
                "evaluation name."
                )
            );
    }

    for (const QString& evaluationName : evaluationNames)
    {
        if (evaluationName.trimmed().isEmpty())
        {
            return technicalFailure(
                QObject::tr(
                    "Loading speaking evaluations failed: invalid class id "
                    "or evaluation name."
                    )
                );
        }
    }

    QStringList requestedValues;
    requestedValues.reserve(evaluationNames.size());
    for (qsizetype index = 0; index < evaluationNames.size(); ++index)
    {
        requestedValues.append(QStringLiteral("(?, ?)"));
    }

    const QString queryText = QStringLiteral(R"(
        WITH requested(ordinal, evaluation_name) AS (
            VALUES %1
        )
        SELECT
            requested.ordinal,
            requested.evaluation_name,
            classes.id AS parent_class_id,
            speaking_evaluations.id AS evaluation_id,
            speaking_eval_data.row_index,
            typeof(speaking_eval_data.row_index)
                AS row_index_storage_type,
            speaking_eval_data.col_0,
            speaking_eval_data.col_1,
            speaking_eval_data.col_2,
            speaking_eval_data.col_3,
            speaking_eval_data.col_4,
            speaking_eval_data.col_5,
            speaking_eval_data.col_6,
            speaking_eval_data.col_7,
            speaking_eval_data.col_8,
            speaking_eval_data.col_9,
            speaking_eval_data.col_10
        FROM classes
        JOIN requested ON 1=1
        LEFT JOIN speaking_evaluations
            ON speaking_evaluations.class_id=classes.id
            AND speaking_evaluations.evaluation_name=requested.evaluation_name
        LEFT JOIN speaking_eval_data
            ON speaking_eval_data.evaluation_id=speaking_evaluations.id
        WHERE classes.id=?
        ORDER BY requested.ordinal, speaking_eval_data.row_index
    )").arg(requestedValues.join(QStringLiteral(", ")));

    QSqlQuery query(m_database);
    query.prepare(queryText);
    for (qsizetype index = 0; index < evaluationNames.size(); ++index)
    {
        query.addBindValue(index);
        query.addBindValue(evaluationNames.at(index));
    }
    query.addBindValue(classId);

    const QString identity = QObject::tr(
        "class id %1, %2 requested speaking evaluations"
        )
        .arg(classId)
        .arg(evaluationNames.size());
    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading speaking evaluations"),
        identity
        );
    if (!loaded)
    {
        return technicalFailure(loaded.error().userMessage());
    }

    bool parentClassFound = false;
    while (query.next())
    {
        bool ordinalValid = false;
        const int ordinal = query.value(0).toInt(&ordinalValid);
        if (!ordinalValid || ordinal < 0 || ordinal >= evaluations.size())
        {
            return technicalFailure(
                QObject::tr(
                    "Loading speaking evaluations failed: the database "
                    "returned an invalid requested evaluation position."
                    )
                );
        }

        if (query.value(2).isNull())
        {
            return technicalFailure(
                QObject::tr(
                    "Loading speaking evaluations failed for %1: invalid "
                    "parent class result."
                    ).arg(identity)
                );
        }
        parentClassFound = true;

        // Keep the legacy batch behavior for SQL NULL row indexes: they are
        // skipped along with the empty LEFT JOIN row for a missing header.
        if (query.value(4).isNull())
        {
            continue;
        }

        if (query.value(5).toString() != QStringLiteral("integer"))
        {
            return technicalFailure(
                QObject::tr(
                    "Loading speaking evaluations failed for %1: "
                    "invalid row index storage type."
                    ).arg(identity)
                );
        }

        QStringList row;
        row.reserve(SpeakingEval::ColumnCount);
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            row.append(query.value(6 + column).toString());
        }
        evaluations[ordinal].rows.append(std::move(row));
    }

    if (query.lastError().isValid())
    {
        return technicalFailure(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading speaking evaluations"),
                queryText,
                identity
                ).userMessage()
            );
    }

    if (!parentClassFound)
    {
        return std::unexpected(ReadFailure{
            .kind = ReadFailure::Kind::ParentClassMissing,
            .diagnostic = QObject::tr(
                "Loading speaking evaluations failed: parent class id %1 "
                "was not found."
                ).arg(classId)
        });
    }

    return evaluations;
}

Result<QList<SpeakingEvalScore>> SpeakingEvalRepository::buildRosterScoreImport(
    int classId,
    const QString& evaluationName
    )
{
    QList<SpeakingEvalScore> scores;

    const Result<SpeakingEvalRows> rows =
        loadSpeakingEval(
            classId,
            evaluationName
            );

    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    if (rows->isEmpty())
    {
        return scores;
    }

    const std::array<int, NextDomain::SpeakingEvaluationCriterionCount>
        scoreColumns{
        SpeakingEval::toInt(SpeakingEvalColumn::Grammar),
        SpeakingEval::toInt(SpeakingEvalColumn::Pronunciation),
        SpeakingEval::toInt(SpeakingEvalColumn::Fluency),
        SpeakingEval::toInt(SpeakingEvalColumn::Manner),
        SpeakingEval::toInt(SpeakingEvalColumn::Content),
        SpeakingEval::toInt(SpeakingEvalColumn::OverallEffort)
    };

    for (const QStringList& row : *rows)
    {
        if (row.size() < SpeakingEval::ColumnCount)
        {
            continue;
        }

        const QString englishName =
            row[SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)]
                .trimmed();

        const QString koreanName =
            row[SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)]
                .trimmed();

        if (englishName.isEmpty() || koreanName.isEmpty())
        {
            continue;
        }

        NextDomain::SpeakingEvaluationComponentScores componentScores{};
        for (std::size_t index = 0; index < scoreColumns.size(); ++index)
        {
            const QString value =
                row[scoreColumns[index]].trimmed();
            const QByteArray label = value.toLatin1();
            componentScores[index] =
                NextDomain::speakingEvaluationGradeFromLabel(
                    std::string_view(
                        label.constData(),
                        static_cast<std::size_t>(label.size())
                        )
                );
        }

        QString finalGrade =
            QStringLiteral("N/A");
        const auto overallGrade =
            NextDomain::calculateOverallSpeakingEvaluationGrade(
                componentScores
                );
        if (overallGrade)
        {
            const auto label =
                NextDomain::speakingEvaluationGradeLabel(*overallGrade);
            if (label)
            {
                finalGrade = QString::fromLatin1(
                    label->data(),
                    static_cast<qsizetype>(label->size())
                    );
            }
            else
            {
                finalGrade = QStringLiteral("N/A");
            }
        }

        scores.append(
            {
                englishName,
                koreanName,
                finalGrade
            }
            );
    }

    return scores;
}

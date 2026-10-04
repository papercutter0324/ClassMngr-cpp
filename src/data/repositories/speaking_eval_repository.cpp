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

namespace NextDomain = ClassMngr::Next::Domain;

SpeakingEvalRepository::SpeakingEvalRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Status SpeakingEvalRepository::saveSpeakingEval(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    )
{
    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return std::unexpected(
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
    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting speaking evaluation transaction failed for %1: %2")
                .arg(identity, m_database.lastError().text())
            );
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

    int evaluationId = -1;

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
        return statement;
    }

    if (query.next())
    {
        evaluationId =
            query.value("id").toInt();
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
            return statement;
        }

        evaluationId =
            query.lastInsertId().toInt();

        if (evaluationId <= 0)
        {
            return std::unexpected(
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
            return statement;
        }
    }

    query.prepare(R"(
        SELECT *
        FROM speaking_eval_data
        WHERE evaluation_id=?
    )");

    query.addBindValue(evaluationId);

    statement = execute(QObject::tr("Loading speaking evaluation rows"));
    if (!statement)
    {
        return statement;
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

        existingRows.insert(
            query.value("row_index").toInt(),
            values
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
            return statement;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
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
        return rows;
    }

    const int evaluationId =
        query.value("id").toInt();

    query.prepare(R"(
        SELECT *
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

        QStringList row;
        row.reserve(SpeakingEval::ColumnCount);
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            row.append(query.value(4 + column).toString());
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
            const std::string_view label =
                NextDomain::speakingEvaluationGradeLabel(*overallGrade);
            finalGrade = QString::fromLatin1(
                label.data(),
                static_cast<qsizetype>(label.size())
                );
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

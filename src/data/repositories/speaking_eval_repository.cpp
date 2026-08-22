#include "speaking_eval_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QHash>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>

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

    const QHash<QString, int> gradeToNumber{
        { QStringLiteral("C"), 1 },
        { QStringLiteral("B"), 2 },
        { QStringLiteral("B+"), 3 },
        { QStringLiteral("A"), 4 },
        { QStringLiteral("A+"), 5 }
    };

    const QHash<int, QString> numberToGrade{
        { 1, QStringLiteral("C") },
        { 2, QStringLiteral("B") },
        { 3, QStringLiteral("B+") },
        { 4, QStringLiteral("A") },
        { 5, QStringLiteral("A+") }
    };

    const QList<int> scoreColumns{
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

        QList<int> numericScores;
        bool valid = true;

        for (int column : scoreColumns)
        {
            const QString value =
                row[column].trimmed();

            if (!gradeToNumber.contains(value))
            {
                valid = false;
                break;
            }

            numericScores.append(
                gradeToNumber.value(value)
                );
        }

        QString finalGrade =
            QStringLiteral("N/A");

        if (valid && numericScores.size() == scoreColumns.size())
        {
            int sum = 0;

            for (int score : numericScores)
            {
                sum += score;
            }

            const double average =
                static_cast<double>(sum)
                / numericScores.size();

            int rounded =
                static_cast<int>(average);

            if (average - rounded >= 0.4)
            {
                ++rounded;
            }

            rounded =
                qBound(
                    1,
                    rounded,
                    5
                    );

            finalGrade =
                numberToGrade.value(
                    rounded,
                    QStringLiteral("N/A")
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

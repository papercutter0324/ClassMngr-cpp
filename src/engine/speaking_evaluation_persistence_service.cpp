#include "classmngr/engine/speaking_evaluation_persistence_service.h"

#include "classmngr/engine/speaking_evaluation_report_service.h"
#include "classmngr/engine/sqlite_database.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace classmngr::engine
{
namespace
{
constexpr std::array<std::string_view, 3> EvaluationColumns{
    "id",
    "class_id",
    "evaluation_name"
};

constexpr std::array<std::string_view, 13> EvaluationDataColumns{
    "evaluation_id",
    "row_index",
    "col_0",
    "col_1",
    "col_2",
    "col_3",
    "col_4",
    "col_5",
    "col_6",
    "col_7",
    "col_8",
    "col_9",
    "col_10"
};

Error error(
    ErrorCode code,
    std::string message
    )
{
    return {
        code,
        std::move(message),
        std::nullopt
    };
}

Error withContext(
    Error source,
    std::string_view action,
    std::string_view identity = {}
    )
{
    std::string message(action);
    if (!identity.empty())
    {
        message += " for ";
        message += identity;
    }
    if (!source.message.empty())
    {
        message += ": ";
        message += source.message;
    }
    source.message = std::move(message);
    return source;
}

std::string trimAsciiWhitespace(
    std::string_view value
    )
{
    std::size_t first = 0;
    while (first < value.size()
        && std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
        && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

Status validateArguments(
    int classId,
    std::string_view evaluationName,
    std::string_view action
    )
{
    if (classId <= 0)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            std::string(action) + " requires a positive class id."
            ));
    }
    if (trimAsciiWhitespace(evaluationName).empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            std::string(action) + " requires a non-blank evaluation name."
            ));
    }

    return {};
}

bool hasExpectedColumns(
    const SqliteQueryResult& result,
    const auto& expected
    )
{
    if (result.columnNames.size() != expected.size())
    {
        return false;
    }

    return std::equal(
        result.columnNames.begin(),
        result.columnNames.end(),
        expected.begin(),
        [](const std::string& actual, std::string_view expectedName)
        {
            return actual == expectedName;
        }
        );
}

Status validateColumns(
    const SqliteQueryResult& result,
    const auto& expected,
    std::string_view tableName,
    std::string_view action,
    std::string_view identity
    )
{
    if (hasExpectedColumns(result, expected))
    {
        return {};
    }

    return std::unexpected(withContext(
        error(
            ErrorCode::Schema,
            "SQLite returned an unexpected " + std::string(tableName)
                + " column shape."
            ),
        action,
        identity
        ));
}

Result<std::int64_t> integerValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    const auto* integer = std::get_if<std::int64_t>(&value);
    if (integer == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-integer " + std::string(column)
                + " value."
            ));
    }

    return *integer;
}

Result<int> positiveIntegerValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    const Result<std::int64_t> integer = integerValue(value, column);
    if (!integer)
    {
        return std::unexpected(integer.error());
    }
    if (*integer <= 0
        || *integer > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid " + std::string(column) + " value."
            ));
    }

    return static_cast<int>(*integer);
}

Result<std::string> textValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    if (const auto* text = std::get_if<std::string>(&value); text != nullptr)
    {
        return *text;
    }
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::string{};
    }

    return std::unexpected(error(
        ErrorCode::Schema,
        "SQLite returned a non-text " + std::string(column) + " value."
        ));
}

std::string evaluationIdentity(
    int classId,
    std::string_view evaluationName
    )
{
    return "evaluation '" + std::string(evaluationName)
        + "' for class id " + std::to_string(classId);
}

std::string loadIdentity(
    int classId,
    std::string_view evaluationName
    )
{
    return "class id " + std::to_string(classId)
        + ", evaluation '" + std::string(evaluationName) + "'";
}

Result<int> evaluationIdFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != EvaluationColumns.size())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected speaking evaluation row shape."
            ));
    }

    const Result<int> id = positiveIntegerValue(row.values[0], "id");
    if (!id)
    {
        return std::unexpected(id.error());
    }

    const Result<std::int64_t> classId = integerValue(
        row.values[1],
        "class_id"
        );
    if (!classId)
    {
        return std::unexpected(classId.error());
    }

    const Result<std::string> name = textValue(
        row.values[2],
        "evaluation_name"
        );
    if (!name)
    {
        return std::unexpected(name.error());
    }

    return *id;
}

Result<std::optional<int>> findEvaluationId(
    SqliteDatabase& database,
    int classId,
    std::string_view evaluationName,
    std::string_view action,
    std::string_view identity
    )
{
    const Result<SqliteQueryResult> queried = database.query(
        "SELECT * FROM speaking_evaluations "
        "WHERE class_id=? AND evaluation_name=?",
        SqliteParameters{
            SqliteValue{std::int64_t{classId}},
            SqliteValue{std::string(evaluationName)}
        }
        );
    if (!queried)
    {
        return std::unexpected(withContext(
            queried.error(),
            action,
            identity
            ));
    }

    const Status columns = validateColumns(
        *queried,
        EvaluationColumns,
        "speaking evaluation",
        action,
        identity
        );
    if (!columns)
    {
        return std::unexpected(columns.error());
    }

    if (queried->rows.empty())
    {
        return std::optional<int>{};
    }

    const Result<int> id = evaluationIdFromRow(queried->rows.front());
    if (!id)
    {
        return std::unexpected(withContext(
            id.error(),
            action,
            identity
            ));
    }

    return std::optional<int>{*id};
}

Result<int> lastInsertId(
    SqliteDatabase& database,
    std::string_view action,
    std::string_view identity
    )
{
    const Result<SqliteQueryResult> queried = database.query(
        "SELECT last_insert_rowid()"
        );
    if (!queried)
    {
        return std::unexpected(withContext(
            queried.error(),
            action,
            identity
            ));
    }
    if (queried->rows.size() != 1
        || queried->rows.front().values.size() != 1)
    {
        return std::unexpected(withContext(
            error(
                ErrorCode::Schema,
                "SQLite did not return a valid speaking evaluation record id."
                ),
            action,
            identity
            ));
    }

    const Result<int> id = positiveIntegerValue(
        queried->rows.front().values.front(),
        "speaking evaluation record id"
        );
    if (!id)
    {
        return std::unexpected(withContext(
            id.error(),
            action,
            identity
            ));
    }

    return *id;
}

struct StoredRow
{
    int rowIndex = -1;
    SpeakingEvaluationRow values;
};

Result<std::vector<StoredRow>> loadStoredRows(
    SqliteDatabase& database,
    int evaluationId,
    std::string_view action,
    std::string_view identity
    )
{
    const Result<SqliteQueryResult> queried = database.query(
        "SELECT * FROM speaking_eval_data "
        "WHERE evaluation_id=? ORDER BY row_index",
        SqliteParameters{SqliteValue{std::int64_t{evaluationId}}}
        );
    if (!queried)
    {
        return std::unexpected(withContext(
            queried.error(),
            action,
            identity
            ));
    }

    const Status columns = validateColumns(
        *queried,
        EvaluationDataColumns,
        "speaking evaluation data",
        action,
        identity
        );
    if (!columns)
    {
        return std::unexpected(columns.error());
    }

    std::vector<StoredRow> result;
    result.reserve(queried->rows.size());
    for (const SqliteRow& row : queried->rows)
    {
        if (row.values.size() != EvaluationDataColumns.size())
        {
            return std::unexpected(withContext(
                error(
                    ErrorCode::Schema,
                    "SQLite returned an unexpected speaking evaluation data "
                    "row shape."
                    ),
                action,
                identity
                ));
        }

        const Result<int> rowIndex = positiveIntegerValue(
            row.values[0],
            "evaluation_id"
            );
        const Result<std::int64_t> rawRowIndex = integerValue(
            row.values[1],
            "row_index"
            );
        if (!rawRowIndex
            || *rawRowIndex < 0
            || *rawRowIndex > std::numeric_limits<int>::max())
        {
            return std::unexpected(withContext(
                rawRowIndex
                    ? error(
                        ErrorCode::Schema,
                        "SQLite returned an invalid row_index value."
                        )
                    : rawRowIndex.error(),
                action,
                identity
                ));
        }
        if (!rowIndex)
        {
            return std::unexpected(withContext(
                rowIndex.error(),
                action,
                identity
                ));
        }
        if (*rowIndex != evaluationId)
        {
            return std::unexpected(withContext(
                error(
                    ErrorCode::Schema,
                    "SQLite returned a speaking evaluation row for the "
                    "wrong evaluation id."
                    ),
                action,
                identity
                ));
        }

        SpeakingEvaluationRow values;
        values.reserve(SpeakingEvaluationColumnCount);
        for (int column = 0;
             column < SpeakingEvaluationColumnCount;
             ++column)
        {
            const Result<std::string> value = textValue(
                row.values[static_cast<std::size_t>(column + 2)],
                "col_" + std::to_string(column)
                );
            if (!value)
            {
                return std::unexpected(withContext(
                    value.error(),
                    action,
                    identity
                    ));
            }
            values.push_back(*value);
        }

        result.push_back({
            static_cast<int>(*rawRowIndex),
            std::move(values)
        });
    }

    return result;
}

Result<int> ensureEvaluation(
    SqliteDatabase& database,
    int classId,
    std::string_view evaluationName,
    std::string_view identity
    )
{
    const std::string lookupAction = "Loading speaking evaluation";
    const Result<std::optional<int>> found = findEvaluationId(
        database,
        classId,
        evaluationName,
        lookupAction,
        identity
        );
    if (!found)
    {
        return std::unexpected(found.error());
    }
    if (found->has_value())
    {
        return **found;
    }

    const Status inserted = database.execute(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (?, ?)",
        SqliteParameters{
            SqliteValue{std::int64_t{classId}},
            SqliteValue{std::string(evaluationName)}
        }
        );
    if (!inserted)
    {
        return std::unexpected(withContext(
            inserted.error(),
            "Creating speaking evaluation",
            identity
            ));
    }

    return lastInsertId(
        database,
        "Creating speaking evaluation",
        identity
        );
}
} // namespace

SpeakingEvaluationPersistenceService::SpeakingEvaluationPersistenceService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Status SpeakingEvaluationPersistenceService::save(
    int classId,
    std::string_view evaluationName,
    const SpeakingEvaluationRows& rows,
    const std::vector<SpeakingEvaluationCellChange>& dirtyCells
    )
{
    const Status valid = validateArguments(
        classId,
        evaluationName,
        "Saving speaking evaluation"
        );
    if (!valid)
    {
        return valid;
    }

    const std::string normalizedEvaluationName = trimAsciiWhitespace(
        evaluationName
        );
    const std::string identity = evaluationIdentity(
        classId,
        normalizedEvaluationName
        );

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting speaking evaluation transaction",
            identity
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Result<int> evaluationId = ensureEvaluation(
        m_database,
        classId,
        normalizedEvaluationName,
        identity
        );
    if (!evaluationId)
    {
        return std::unexpected(evaluationId.error());
    }

    for (int row = 0; row < SpeakingEvaluationRowCount; ++row)
    {
        const Status ensured = m_database.execute(
            "INSERT OR IGNORE INTO speaking_eval_data "
            "(evaluation_id, row_index) VALUES (?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{*evaluationId}},
                SqliteValue{std::int64_t{row}}
            }
            );
        if (!ensured)
        {
            return std::unexpected(withContext(
                ensured.error(),
                "Ensuring speaking evaluation row",
                identity
                ));
        }
    }

    const Result<std::vector<StoredRow>> storedRows = loadStoredRows(
        m_database,
        *evaluationId,
        "Loading speaking evaluation rows",
        identity
        );
    if (!storedRows)
    {
        return std::unexpected(storedRows.error());
    }

    std::vector<SpeakingEvaluationRow> existingRows(
        static_cast<std::size_t>(SpeakingEvaluationRowCount)
        );
    std::vector<bool> hasExistingRow(
        static_cast<std::size_t>(SpeakingEvaluationRowCount),
        false
        );
    for (const StoredRow& stored : *storedRows)
    {
        if (stored.rowIndex < 0
            || stored.rowIndex >= SpeakingEvaluationRowCount)
        {
            continue;
        }

        existingRows[static_cast<std::size_t>(stored.rowIndex)] =
            stored.values;
        hasExistingRow[static_cast<std::size_t>(stored.rowIndex)] = true;
    }

    std::vector<SpeakingEvaluationCellChange> cellsToUpdate = dirtyCells;
    if (cellsToUpdate.empty())
    {
        cellsToUpdate.reserve(
            static_cast<std::size_t>(SpeakingEvaluationRowCount)
                * static_cast<std::size_t>(SpeakingEvaluationColumnCount)
            );
        for (int row = 0; row < SpeakingEvaluationRowCount; ++row)
        {
            for (int column = 0;
                 column < SpeakingEvaluationColumnCount;
                 ++column)
            {
                cellsToUpdate.push_back({row, column});
            }
        }
    }

    for (const SpeakingEvaluationCellChange& cell : cellsToUpdate)
    {
        if (cell.row < 0
            || cell.row >= SpeakingEvaluationRowCount
            || cell.column < 0
            || cell.column >= SpeakingEvaluationColumnCount)
        {
            continue;
        }

        const std::size_t rowIndex = static_cast<std::size_t>(cell.row);
        const std::size_t columnIndex = static_cast<std::size_t>(cell.column);
        const std::string newValue =
            rowIndex < rows.size()
            && columnIndex < rows[rowIndex].size()
                ? rows[rowIndex][columnIndex]
                : std::string{};

        const std::string oldValue = hasExistingRow[rowIndex]
            && columnIndex < existingRows[rowIndex].size()
            ? existingRows[rowIndex][columnIndex]
            : std::string{};
        if (oldValue == newValue)
        {
            continue;
        }

        const Status updated = m_database.execute(
            "UPDATE speaking_eval_data SET col_"
                + std::to_string(cell.column)
                + "=? WHERE evaluation_id=? AND row_index=?",
            SqliteParameters{
                SqliteValue{newValue},
                SqliteValue{std::int64_t{*evaluationId}},
                SqliteValue{std::int64_t{cell.row}}
            }
            );
        if (!updated)
        {
            return std::unexpected(withContext(
                updated.error(),
                "Updating speaking evaluation cell",
                identity
                ));
        }
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing speaking evaluation",
            identity
            ));
    }

    return {};
}

Result<SpeakingEvaluationRows> SpeakingEvaluationPersistenceService::load(
    int classId,
    std::string_view evaluationName
    )
{
    const Status valid = validateArguments(
        classId,
        evaluationName,
        "Loading speaking evaluation"
        );
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const std::string normalizedEvaluationName = trimAsciiWhitespace(
        evaluationName
        );
    const std::string identity = loadIdentity(
        classId,
        normalizedEvaluationName
        );
    const Result<std::optional<int>> evaluationId = findEvaluationId(
        m_database,
        classId,
        normalizedEvaluationName,
        "Loading speaking evaluation",
        identity
        );
    if (!evaluationId)
    {
        return std::unexpected(evaluationId.error());
    }
    if (!evaluationId->has_value())
    {
        return SpeakingEvaluationRows{};
    }

    const Result<std::vector<StoredRow>> storedRows = loadStoredRows(
        m_database,
        **evaluationId,
        "Loading speaking evaluation rows",
        identity
        );
    if (!storedRows)
    {
        return std::unexpected(storedRows.error());
    }

    SpeakingEvaluationRows result;
    result.reserve(storedRows->size());
    for (const StoredRow& stored : *storedRows)
    {
        result.push_back(stored.values);
    }
    return result;
}

Result<std::vector<SpeakingEvaluationScore>>
SpeakingEvaluationPersistenceService::buildRosterScoreImport(
    int classId,
    std::string_view evaluationName
    )
{
    const Result<SpeakingEvaluationRows> rows = load(
        classId,
        evaluationName
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<SpeakingEvaluationScore> result;
    if (rows->empty())
    {
        return result;
    }

    constexpr std::array<int, SpeakingEvaluationScoreCount> scoreColumns{
        toInt(SpeakingEvaluationColumn::Grammar),
        toInt(SpeakingEvaluationColumn::Pronunciation),
        toInt(SpeakingEvaluationColumn::Fluency),
        toInt(SpeakingEvaluationColumn::Manner),
        toInt(SpeakingEvaluationColumn::Content),
        toInt(SpeakingEvaluationColumn::OverallEffort)
    };

    for (const SpeakingEvaluationRow& row : *rows)
    {
        if (row.size() < static_cast<std::size_t>(
                SpeakingEvaluationColumnCount
                ))
        {
            continue;
        }

        const std::string englishName = trimAsciiWhitespace(
            row[static_cast<std::size_t>(toInt(
                SpeakingEvaluationColumn::EnglishName
                ))]
            );
        const std::string koreanName = trimAsciiWhitespace(
            row[static_cast<std::size_t>(toInt(
                SpeakingEvaluationColumn::KoreanName
                ))]
            );
        if (englishName.empty() || koreanName.empty())
        {
            continue;
        }

        SpeakingEvaluationScores scores;
        for (std::size_t index = 0; index < scoreColumns.size(); ++index)
        {
            scores[index] = trimAsciiWhitespace(
                row[static_cast<std::size_t>(scoreColumns[index])]
                );
        }

        result.push_back({
            englishName,
            koreanName,
            SpeakingEvaluationReportService::overallGrade(scores)
        });
    }

    return result;
}

} // namespace classmngr::engine

#include "classmngr/engine/open_database.h"
#include "classmngr/engine/speaking_evaluation_persistence_service.h"
#include "classmngr/engine/sqlite_database.h"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace classmngr::engine;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationPersistenceServiceTests: "
              << message
              << '\n';
    return false;
}

std::unique_ptr<SqliteDatabase> openDatabase()
{
    auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return nullptr;
    }

    return std::move(*opened);
}

bool seedClass(
    SqliteDatabase& database
    )
{
    return database.execute(
        "INSERT INTO classes (name) VALUES (?)",
        SqliteParameters{SqliteValue{std::string("Speaking class")}}
        ).has_value();
}

SpeakingEvaluationRows emptyRows()
{
    return SpeakingEvaluationRows(
        static_cast<std::size_t>(SpeakingEvaluationRowCount),
        SpeakingEvaluationRow(
            static_cast<std::size_t>(SpeakingEvaluationColumnCount)
            )
        );
}

bool sameCell(
    const SpeakingEvaluationRows& rows,
    int row,
    int column,
    std::string_view expected
    )
{
    return row >= 0
        && column >= 0
        && static_cast<std::size_t>(row) < rows.size()
        && static_cast<std::size_t>(column) < rows[
            static_cast<std::size_t>(row)
            ].size()
        && rows[static_cast<std::size_t>(row)][
            static_cast<std::size_t>(column)
            ] == expected;
}
} // namespace

int main()
{
    bool passed = true;

    auto database = openDatabase();
    passed &= expect(
        database != nullptr,
        "OpenDatabase failed for the primary fixture"
        );
    if (database == nullptr)
    {
        return 1;
    }

    passed &= expect(
        seedClass(*database),
        "class fixture could not be created"
        );

    SpeakingEvaluationPersistenceService service(*database);
    SpeakingEvaluationRows rows = emptyRows();

    const auto invalidClassSave = service.save(0, "Evaluation", rows);
    const auto invalidNameSave = service.save(1, " \t ", rows);
    const auto invalidClassLoad = service.load(0, "Evaluation");
    const auto invalidNameLoad = service.load(1, " \t ");
    const auto invalidClassImport = service.buildRosterScoreImport(
        0,
        "Evaluation"
        );
    passed &= expect(
        !invalidClassSave
            && invalidClassSave.error().code == ErrorCode::InvalidArgument
            && !invalidNameSave
            && invalidNameSave.error().code == ErrorCode::InvalidArgument
            && !invalidClassLoad
            && invalidClassLoad.error().code == ErrorCode::InvalidArgument
            && !invalidNameLoad
            && invalidNameLoad.error().code == ErrorCode::InvalidArgument
            && !invalidClassImport
            && invalidClassImport.error().code == ErrorCode::InvalidArgument,
        "invalid speaking-evaluation arguments were accepted"
        );

    rows[0][0] = "1";
    rows[0][1] = " 민지 🌟 ";
    rows[0][2] = " 학생 ";
    rows[0][4] = "keep this value";
    rows[0][9] = "설명: 첫 번째 평가";
    rows[0][10] = "备注 📝";
    rows[1][0] = "2";
    rows[1][1] = "두 번째";
    rows[1][2] = "학생 둘";

    const auto saved = service.save(1, "  평가 🌏  ", rows);
    const auto loaded = service.load(1, "평가 🌏");
    passed &= expect(
        saved
            && loaded
            && loaded->size() == static_cast<std::size_t>(
                SpeakingEvaluationRowCount
                )
            && loaded->front().size() == static_cast<std::size_t>(
                SpeakingEvaluationColumnCount
                )
            && sameCell(*loaded, 0, 1, " 민지 🌟 ")
            && sameCell(*loaded, 0, 2, " 학생 ")
            && sameCell(*loaded, 0, 9, "설명: 첫 번째 평가")
            && sameCell(*loaded, 0, 10, "备注 📝"),
        "UTF-8 speaking-evaluation round-trip or row creation failed"
        );

    SpeakingEvaluationRows dirtyRows = rows;
    dirtyRows[0][0] = "changed only";
    dirtyRows[0][4] = "must remain unchanged";
    dirtyRows[1][1] = "dirty update";
    const auto dirtySaved = service.save(
        1,
        "평가 🌏",
        dirtyRows,
        {
            {0, 0},
            {1, 1},
            {-1, 0},
            {SpeakingEvaluationRowCount, 0},
            {0, -1},
            {0, SpeakingEvaluationColumnCount}
        }
        );
    const auto dirtyLoaded = service.load(1, "평가 🌏");
    passed &= expect(
        dirtySaved
            && dirtyLoaded
            && sameCell(*dirtyLoaded, 0, 0, "changed only")
            && sameCell(*dirtyLoaded, 0, 4, "keep this value")
            && sameCell(*dirtyLoaded, 1, 1, "dirty update"),
        "dirty-cell save did not update only valid requested cells"
        );

    dirtyRows[0][4] = "full save update";
    const auto fullSaved = service.save(1, "평가 🌏", dirtyRows);
    const auto fullLoaded = service.load(1, "평가 🌏");
    passed &= expect(
        fullSaved
            && fullLoaded
            && sameCell(*fullLoaded, 0, 4, "full save update"),
        "empty dirty-cell list did not perform a full save"
        );

    const auto missingEvaluation = service.load(1, "not present");
    const auto missingImport = service.buildRosterScoreImport(
        1,
        "not present"
        );
    passed &= expect(
        missingEvaluation
            && missingEvaluation->empty()
            && missingImport
            && missingImport->empty(),
        "missing evaluation did not produce an empty result"
        );

    SpeakingEvaluationRows importRows = emptyRows();
    importRows[0][toInt(SpeakingEvaluationColumn::EnglishName)] = " Alice ";
    importRows[0][toInt(SpeakingEvaluationColumn::KoreanName)] = " 학생 ";
    for (const int column : {
             toInt(SpeakingEvaluationColumn::Grammar),
             toInt(SpeakingEvaluationColumn::Pronunciation),
             toInt(SpeakingEvaluationColumn::Fluency),
             toInt(SpeakingEvaluationColumn::Manner),
             toInt(SpeakingEvaluationColumn::Content),
             toInt(SpeakingEvaluationColumn::OverallEffort)
         })
    {
        importRows[0][static_cast<std::size_t>(column)] = " A+ ";
    }
    importRows[1][toInt(SpeakingEvaluationColumn::EnglishName)] = "No Korean";
    importRows[2][toInt(SpeakingEvaluationColumn::KoreanName)] = "No English";

    const auto importSaved = service.save(1, "Import", importRows);
    const auto imported = service.buildRosterScoreImport(1, "Import");
    passed &= expect(
        importSaved
            && imported
            && imported->size() == 1
            && imported->front().englishName == "Alice"
            && imported->front().koreanName == "학생"
            && imported->front().finalGrade == "A+",
        "roster score import did not filter names or calculate the grade"
        );

    auto readFailureDatabase = openDatabase();
    passed &= expect(
        readFailureDatabase != nullptr
            && seedClass(*readFailureDatabase),
        "read-failure fixture could not be created"
        );
    if (readFailureDatabase != nullptr)
    {
        SpeakingEvaluationPersistenceService readFailureService(
            *readFailureDatabase
            );
        passed &= expect(
            readFailureDatabase->execute(
                "DROP TABLE speaking_evaluations"
                ).has_value(),
            "read-failure fixture could not drop the evaluation table"
            );
        const auto readFailure = readFailureService.load(1, "Evaluation");
        passed &= expect(
            !readFailure && readFailure.error().code == ErrorCode::Database,
            "missing evaluation table did not return a database error"
            );
    }

    auto rollbackDatabase = openDatabase();
    passed &= expect(
        rollbackDatabase != nullptr
            && seedClass(*rollbackDatabase),
        "rollback fixture could not be created"
        );
    if (rollbackDatabase != nullptr)
    {
        SpeakingEvaluationPersistenceService rollbackService(
            *rollbackDatabase
            );
        SpeakingEvaluationRows originalRows = emptyRows();
        originalRows[0][0] = "Original A";
        originalRows[1][0] = "Original B";
        passed &= expect(
            rollbackService.save(1, "Rollback", originalRows).has_value(),
            "rollback fixture could not save its original evaluation"
            );
        passed &= expect(
            rollbackDatabase->execute(
                "CREATE TRIGGER reject_speaking_eval_update "
                "BEFORE UPDATE OF col_0 ON speaking_eval_data "
                "WHEN NEW.row_index = 1 AND NEW.col_0 = 'Reject' "
                "BEGIN "
                "SELECT RAISE(ABORT, 'injected speaking evaluation failure'); "
                "END"
                ).has_value(),
            "rollback trigger could not be created"
            );

        SpeakingEvaluationRows changedRows = originalRows;
        changedRows[0][0] = "Changed A";
        changedRows[1][0] = "Reject";
        const auto failedSave = rollbackService.save(
            1,
            "Rollback",
            changedRows,
            {{0, 0}, {1, 0}}
            );
        const auto rolledBack = rollbackService.load(1, "Rollback");
        passed &= expect(
            !failedSave
                && failedSave.error().code == ErrorCode::Database
                && rolledBack
                && sameCell(*rolledBack, 0, 0, "Original A")
                && sameCell(*rolledBack, 1, 0, "Original B"),
            "injected write failure did not roll back the speaking evaluation"
            );
    }

    auto schemaDatabase = openDatabase();
    passed &= expect(
        schemaDatabase != nullptr
            && seedClass(*schemaDatabase),
        "schema-shape fixture could not be created"
        );
    if (schemaDatabase != nullptr)
    {
        SpeakingEvaluationPersistenceService schemaService(*schemaDatabase);
        passed &= expect(
            schemaDatabase->execute(
                "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
                "VALUES (1, 'Malformed')"
                ).has_value()
                && schemaDatabase->execute(
                    "DROP TABLE speaking_eval_data"
                    ).has_value()
                && schemaDatabase->execute(
                    "CREATE TABLE speaking_eval_data ("
                    "evaluation_id INTEGER, row_index INTEGER, col_0 TEXT"
                    ")"
                    ).has_value(),
            "malformed speaking-evaluation data table could not be created"
            );
        const auto schemaFailure = schemaService.load(1, "Malformed");
        passed &= expect(
            !schemaFailure && schemaFailure.error().code == ErrorCode::Schema,
            "malformed speaking-evaluation data shape was not typed as schema"
            );
    }

    return passed ? 0 : 1;
}

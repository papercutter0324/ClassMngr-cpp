#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/roster_service.h"
#include "classmngr/engine/open_database.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using classmngr::engine::ClassRepository;
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::Roster;
using classmngr::engine::RosterService;
using classmngr::engine::SqliteParameters;
using classmngr::engine::SqliteValue;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineRosterServiceTests: "
              << message
              << '\n';
    return false;
}

bool sameRoster(
    const Roster& lhs,
    const Roster& rhs
    )
{
    return lhs.columns == rhs.columns
        && lhs.columnWidths == rhs.columnWidths
        && lhs.rows == rhs.rows;
}

bool hasPersistedCells(
    classmngr::engine::SqliteDatabase& database,
    int classId
    )
{
    const auto rows = database.query(
        "SELECT row_index, col_index, value FROM roster_data "
        "WHERE class_id=? ORDER BY row_index, col_index",
        SqliteParameters{SqliteValue{std::int64_t{classId}}}
        );
    if (!rows || rows->rows.size() != 3)
    {
        return false;
    }

    const std::vector<std::vector<std::string>> expected{
        {"0", "0", "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99"},
        {"0", "2", "\xE2\x9C\x93"},
        {"3", "1", "\xE5\xAD\xA6\xE7\x94\x9F"}
    };

    for (std::size_t row = 0; row < expected.size(); ++row)
    {
        if (rows->rows[row].values.size() != expected[row].size())
        {
            return false;
        }
        for (std::size_t column = 0; column < expected[row].size(); ++column)
        {
            const auto* value = std::get_if<std::int64_t>(
                &rows->rows[row].values[column]
                );
            if (column == 2)
            {
                const auto* text = std::get_if<std::string>(
                    &rows->rows[row].values[column]
                    );
                if (text == nullptr || *text != expected[row][column])
                {
                    return false;
                }
            }
            else if (value == nullptr
                     || std::to_string(*value) != expected[row][column])
            {
                return false;
            }
        }
    }

    return true;
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineRosterServiceTests: OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    ClassRepository classRepository(database);
    const auto created = classRepository.create("Roster");
    if (!created)
    {
        std::cerr << "ClassMngrEngineRosterServiceTests: class creation failed\n";
        return 1;
    }

    const int classId = *created;
    RosterService service(database);
    bool passed = true;

    const auto invalidLoad = service.load(0);
    passed &= expect(
        !invalidLoad && invalidLoad.error().code == ErrorCode::InvalidArgument,
        "non-positive load class id was not rejected"
        );

    Roster invalidSaveRoster;
    invalidSaveRoster.columns = {"English"};
    const auto invalidSave = service.save(-1, invalidSaveRoster);
    passed &= expect(
        !invalidSave && invalidSave.error().code == ErrorCode::InvalidArgument,
        "non-positive save class id was not rejected"
        );

    const auto missingLoad = service.load(classId + 1);
    passed &= expect(
        !missingLoad && missingLoad.error().code == ErrorCode::NotFound,
        "missing load class did not return a typed not-found error"
        );
    const auto missingSave = service.save(classId + 1, invalidSaveRoster);
    passed &= expect(
        !missingSave && missingSave.error().code == ErrorCode::NotFound,
        "missing save class did not return a typed not-found error"
        );

    const std::string columnName = "\xEC\x9D\xB4\xEB\xA6\x84";
    const std::string notesColumn = "\xE5\xA4\x87\xE6\xB3\xA8";
    const std::string statusColumn = "\xE7\x8A\xB6\xE6\x85\x8B";
    const std::string studentName = "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99";
    const std::string checked = "\xE2\x9C\x93";
    const std::string note = "\xE5\xAD\xA6\xE7\x94\x9F";

    Roster source;
    source.columns = {columnName, notesColumn, statusColumn};
    source.columnWidths = {42};
    source.rows = {
        {studentName, "", checked, "ignored extra cell"},
        {},
        {},
        {"", note, ""}
    };

    passed &= expect(
        service.save(classId, source).has_value(),
        "UTF-8 sparse roster save failed"
        );

    Roster expectedSource;
    expectedSource.columns = source.columns;
    expectedSource.columnWidths = {42, 0, 0};
    expectedSource.rows = {
        {studentName, "", checked},
        {"", "", ""},
        {"", "", ""},
        {"", note, ""}
    };

    const auto loadedSource = service.load(classId);
    passed &= expect(
        loadedSource && sameRoster(*loadedSource, expectedSource),
        "UTF-8 sparse roster load or width fallback changed the data"
        );
    passed &= expect(
        hasPersistedCells(database, classId),
        "save did not persist only non-empty declared cells"
        );

    Roster replacement;
    replacement.columns = {columnName, statusColumn};
    replacement.columnWidths = {7, 8};
    replacement.rows = {{"\xEC\x83\x88\xEA\xB0\x92", ""}};
    passed &= expect(
        service.save(classId, replacement).has_value(),
        "roster replacement save failed"
        );

    const auto loadedReplacement = service.load(classId);
    passed &= expect(
        loadedReplacement && sameRoster(*loadedReplacement, replacement),
        "roster replacement did not remove prior columns or data"
        );

    const std::string trigger =
        "CREATE TRIGGER reject_roster_data_insert "
        "BEFORE INSERT ON roster_data "
        "BEGIN SELECT RAISE(ABORT, 'forced roster failure'); END";
    passed &= expect(
        database.execute(trigger).has_value(),
        "forced roster failure trigger could not be created"
        );

    Roster failedReplacement;
    failedReplacement.columns = {"Should not replace"};
    failedReplacement.columnWidths = {99};
    failedReplacement.rows = {{"Should not persist"}};
    const auto failedSave = service.save(classId, failedReplacement);
    passed &= expect(
        !failedSave && failedSave.error().code == ErrorCode::Database,
        "forced roster SQL failure did not fail with a database error"
        );

    const auto afterFailedSave = service.load(classId);
    passed &= expect(
        afterFailedSave && sameRoster(*afterFailedSave, replacement),
        "roster columns and data were not restored after a failed save"
        );

    passed &= expect(
        database.execute("DROP TRIGGER reject_roster_data_insert").has_value(),
        "forced roster failure trigger could not be removed"
        );
    passed &= expect(
        database.execute("PRAGMA ignore_check_constraints = ON").has_value(),
        "SQLite did not enable malformed-roster fixture setup"
        );
    passed &= expect(
        database.execute(
            "INSERT INTO roster_data "
            "(class_id, row_index, col_index, value) VALUES (?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{classId}},
                SqliteValue{std::int64_t{-1}},
                SqliteValue{std::int64_t{0}},
                SqliteValue{std::string("negative row")}
            }
            ).has_value(),
        "negative-row fixture could not be inserted"
        );
    passed &= expect(
        database.execute(
            "INSERT INTO roster_data "
            "(class_id, row_index, col_index, value) VALUES (?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{classId}},
                SqliteValue{std::int64_t{classId}},
                SqliteValue{std::int64_t{-1}},
                SqliteValue{std::string("negative column")}
            }
            ).has_value(),
        "negative-column fixture could not be inserted"
        );
    passed &= expect(
        database.execute(
            "INSERT INTO roster_data "
            "(class_id, row_index, col_index, value) VALUES (?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::int64_t{classId}},
                SqliteValue{std::int64_t{0}},
                SqliteValue{std::int64_t{99}},
                SqliteValue{std::string("out of range column")}
            }
            ).has_value(),
        "out-of-range-column fixture could not be inserted"
        );
    passed &= expect(
        database.execute("PRAGMA ignore_check_constraints = OFF").has_value(),
        "SQLite did not restore roster check constraints"
        );

    const auto loadedMalformed = service.load(classId);
    passed &= expect(
        loadedMalformed && sameRoster(*loadedMalformed, replacement),
        "malformed negative or out-of-range roster data was not skipped"
        );

    return passed ? 0 : 1;
}

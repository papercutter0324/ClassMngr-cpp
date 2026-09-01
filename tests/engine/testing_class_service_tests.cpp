#include "classmngr/engine/open_database.h"
#include "classmngr/engine/testing_class_service.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::SqliteDatabase;
using classmngr::engine::SqliteParameters;
using classmngr::engine::SqliteValue;
using classmngr::engine::Status;
using classmngr::engine::TestingClass;
using classmngr::engine::TestingClassService;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineTestingClassServiceTests: "
              << message
              << '\n';
    return false;
}

TestingClass validTestingClass()
{
    TestingClass testingClass;
    testingClass.classId = 123;
    testingClass.name = " \t테스트 클래스 \r\n";
    testingClass.grade = " M1 ";
    testingClass.level = " Mixed (All) ";
    testingClass.room = "\n 시험실 \t";
    testingClass.teacherId = 0;
    testingClass.classColor = " \t";
    testingClass.fontColor = "\r";
    testingClass.notes = " 메모 UTF-8 한글 ";
    return testingClass;
}

bool isNullValue(
    const SqliteValue& value
    )
{
    return std::holds_alternative<std::monostate>(value);
}

std::int64_t lastInsertId(
    SqliteDatabase& database
    )
{
    const auto result = database.query("SELECT last_insert_rowid()");
    if (!result
        || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return -1;
    }

    const auto* value = std::get_if<std::int64_t>(
        &result->rows.front().values.front()
        );
    return value == nullptr ? -1 : *value;
}

bool countEquals(
    SqliteDatabase& database,
    std::string_view sql,
    std::int64_t expected,
    const SqliteParameters& parameters = {}
    )
{
    const auto result = database.query(sql, parameters);
    if (!result
        || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return false;
    }

    const auto* value = std::get_if<std::int64_t>(
        &result->rows.front().values.front()
        );
    return value != nullptr && *value == expected;
}

bool hasId(
    SqliteDatabase& database,
    std::string_view table,
    std::string_view column,
    int id
    )
{
    return countEquals(
        database,
        "SELECT COUNT(*) FROM " + std::string(table)
            + " WHERE " + std::string(column) + "=?",
        1,
        SqliteParameters{
            SqliteValue{std::int64_t{id}}
        }
        );
}

bool rowTextEquals(
    const SqliteValue& value,
    std::string_view expected
    )
{
    const auto* text = std::get_if<std::string>(&value);
    return text != nullptr && *text == expected;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        classmngr::engine::testingClassMixedLevels()
            == std::vector<std::string>{
                "Mixed (All)", "Mixed (High)", "Mixed (Low)"
            },
        "testing-class mixed-level choices changed"
        );
    passed &= expect(
        classmngr::engine::testingClassGrades()
            == std::vector<std::string>{"M1", "M2", "Mixed"},
        "testing-class grade choices changed"
        );
    passed &= expect(
        classmngr::engine::testingClassLevelsForGrade("M1")
            == std::vector<std::string>{
                "Mixed (All)", "Mixed (High)", "Mixed (Low)",
                "Song's", "Major", "Solis", "Galaxia", "Elephantus"
            },
        "M1 testing-class level choices changed"
        );
    passed &= expect(
        classmngr::engine::testingClassLevelsForGrade("M2")
            == std::vector<std::string>{
                "Mixed (All)", "Mixed (High)", "Mixed (Low)",
                "Song's", "Major", "Tigris", "Leo", "Ursa"
            },
        "M2 testing-class level choices changed"
        );
    passed &= expect(
        classmngr::engine::testingClassLevelsForGrade("Mixed")
            == std::vector<std::string>{
                "Mixed (All)", "Mixed (High)", "Mixed (Low)"
            },
        "mixed testing-class level choices changed"
        );

    const auto opened = OpenDatabase::execute(":memory:");
    passed &= expect(
        opened && *opened != nullptr,
        "OpenDatabase failed for testing-class service test"
        );
    if (!opened || *opened == nullptr)
    {
        return 1;
    }

    auto& database = **opened;
    TestingClassService service(database);

    for (const std::string_view field : {
             "name", "grade", "level", "room"})
    {
        TestingClass invalid = validTestingClass();
        if (field == "name")
        {
            invalid.name = " \t\r\n";
        }
        else if (field == "grade")
        {
            invalid.grade = " \t\r\n";
        }
        else if (field == "level")
        {
            invalid.level = " \t\r\n";
        }
        else
        {
            invalid.room = " \t\r\n";
        }

        const auto rejected = service.create(invalid);
        passed &= expect(
            !rejected && rejected.error().code == ErrorCode::InvalidArgument,
            "blank required testing-class data was accepted"
            );
    }

    for (const auto& assignment : {
             std::pair<std::string_view, std::string_view>{"Monday", ""},
             std::pair<std::string_view, std::string_view>{"", "09:05"},
             std::pair<std::string_view, std::string_view>{"Funday", "09:05"},
             std::pair<std::string_view, std::string_view>{"Monday", "9:05"},
             std::pair<std::string_view, std::string_view>{"Monday", "24:00"},
             std::pair<std::string_view, std::string_view>{"Monday", "09:60"}})
    {
        const auto rejected = service.create(
            validTestingClass(),
            assignment.first,
            assignment.second
            );
        passed &= expect(
            !rejected && rejected.error().code == ErrorCode::InvalidArgument,
            "partial or invalid testing assignment was accepted"
            );
    }

    const auto created = service.create(
        validTestingClass(),
        " tUeSdAy ",
        " 09:05 "
        );
    passed &= expect(
        created && *created > 0,
        "testing-class creation did not return a positive id"
        );

    const auto noAssignment = [&] {
        TestingClass testingClass = validTestingClass();
        testingClass.name = "No assignment";
        testingClass.grade = "M2";
        testingClass.level = "Ursa";
        testingClass.room = "Room 2";
        testingClass.notes.clear();
        return service.create(testingClass, " \t", "\n");
    }();
    passed &= expect(
        noAssignment && *noAssignment > 0,
        "testing-class creation without an assignment failed"
        );

    const int firstId = created ? *created : -1;
    const int noAssignmentId = noAssignment ? *noAssignment : -1;
    if (created)
    {
        const auto loaded = service.get(*created);
        passed &= expect(
            loaded
                && loaded->classId == *created
                && loaded->name == "테스트 클래스"
                && loaded->grade == "M1"
                && loaded->level == "Mixed (All)"
                && loaded->room == "시험실"
                && loaded->teacherId == -1
                && loaded->classColor == "#FFFFFF"
                && loaded->fontColor == "#000000"
                && loaded->notes == " 메모 UTF-8 한글 ",
            "testing-class UTF-8, trimming, NULL teacher, or color defaults "
            "did not round trip"
            );

        const auto details = database.query(
            "SELECT teacher_id, class_grade, class_level, class_color, "
            "font_color, notes, time_filler_activities FROM class_info "
            "WHERE class_id=?",
            SqliteParameters{
                SqliteValue{std::int64_t{*created}}
            }
            );
        passed &= expect(
            details
                && details->rows.size() == 1
                && details->rows.front().values.size() == 7
                && isNullValue(details->rows.front().values[0])
                && rowTextEquals(details->rows.front().values[1], "M1")
                && rowTextEquals(
                    details->rows.front().values[2],
                    "Mixed (All)"
                    )
                && rowTextEquals(details->rows.front().values[3], "")
                && rowTextEquals(details->rows.front().values[4], "")
                && rowTextEquals(
                    details->rows.front().values[5],
                    " 메모 UTF-8 한글 "
                    )
                && rowTextEquals(details->rows.front().values[6], ""),
            "testing-class create did not persist the expected class_info "
            "values"
            );

        const auto assignment = database.query(
            "SELECT day, start_time, room FROM schedule_testing_blocks "
            "WHERE class_id=?",
            SqliteParameters{
                SqliteValue{std::int64_t{*created}}
            }
            );
        passed &= expect(
            assignment
                && assignment->rows.size() == 1
                && assignment->rows.front().values.size() == 3
                && rowTextEquals(assignment->rows.front().values[0], "Tuesday")
                && rowTextEquals(assignment->rows.front().values[1], "09:05")
                && rowTextEquals(assignment->rows.front().values[2], ""),
            "testing-class assignment was not canonicalized and persisted"
            );
    }
    if (noAssignment)
    {
        passed &= expect(
            countEquals(
                database,
                "SELECT COUNT(*) FROM schedule_testing_blocks WHERE class_id=?",
                0,
                SqliteParameters{
                    SqliteValue{std::int64_t{*noAssignment}}
                }
                ),
            "blank testing assignment unexpectedly created a schedule row"
            );
    }

    const Status regularInserted = database.execute(
        "INSERT INTO classes (name) VALUES (?)",
        SqliteParameters{
            SqliteValue{std::string("Regular class")}
        }
        );
    passed &= expect(
        regularInserted.has_value(),
        "regular-class fixture could not be inserted"
        );
    const std::int64_t regularId = lastInsertId(database);

    passed &= expect(
        service.isTestingClass(0).has_value()
            && !*service.isTestingClass(0),
        "nonpositive testing-class membership did not return false"
        );
    passed &= expect(
        firstId > 0
            && service.isTestingClass(firstId)
            && *service.isTestingClass(firstId),
        "created testing-class membership was not reported"
        );
    passed &= expect(
        regularId > 0
            && service.isTestingClass(static_cast<int>(regularId))
            && !*service.isTestingClass(static_cast<int>(regularId)),
        "regular class was reported as a testing class"
        );

    const auto listed = service.list();
    passed &= expect(
        listed && listed->size() == (created && noAssignment ? 2U : 0U),
        "testing-class listing did not exclude regular classes"
        );
    if (listed && listed->size() == 2)
    {
        passed &= expect(
            listed->front().classId == firstId
                && listed->back().classId == noAssignmentId,
            "testing classes were not ordered by grade, level, name, and id"
            );
    }

    if (created)
    {
        passed &= expect(
            database.execute(
                "UPDATE class_info SET time_filler_activities=? "
                "WHERE class_id=?",
                SqliteParameters{
                    SqliteValue{std::string("preserve filler")},
                    SqliteValue{std::int64_t{*created}}
                }
                ).has_value(),
            "testing-class update preservation fixture could not be written"
            );

        TestingClass updated = validTestingClass();
        updated.classId = *created;
        updated.name = " Updated 测试 ";
        updated.grade = " M2 ";
        updated.level = " Leo ";
        updated.room = " Room 3 ";
        updated.teacherId = -7;
        updated.classColor = " #112233 ";
        updated.fontColor = " #ABCDEF ";
        updated.notes = " updated notes 한글 ";
        passed &= expect(
            service.update(updated).has_value(),
            "testing-class update failed"
            );

        const auto loaded = service.get(*created);
        passed &= expect(
            loaded
                && loaded->name == "Updated 测试"
                && loaded->grade == "M2"
                && loaded->level == "Leo"
                && loaded->room == "Room 3"
                && loaded->teacherId == -1
                && loaded->classColor == "#112233"
                && loaded->fontColor == "#ABCDEF"
                && loaded->notes == " updated notes 한글 ",
            "testing-class update did not trim or preserve portable fields"
            );
        passed &= expect(
            countEquals(
                database,
                "SELECT COUNT(*) FROM class_info "
                "WHERE class_id=? AND teacher_id IS NULL "
                "AND time_filler_activities=?",
                1,
                SqliteParameters{
                    SqliteValue{std::int64_t{*created}},
                    SqliteValue{std::string("preserve filler")}
                }
                ),
            "testing-class update did not preserve existing filler activities "
            "or NULL teacher semantics"
            );
    }

    TestingClass updateInvalid = validTestingClass();
    updateInvalid.classId = 0;
    const auto invalidUpdate = service.update(updateInvalid);
    passed &= expect(
        !invalidUpdate && invalidUpdate.error().code == ErrorCode::InvalidArgument,
        "testing-class update accepted a nonpositive id"
        );
    updateInvalid.classId = std::numeric_limits<int>::max();
    const auto missingUpdate = service.update(updateInvalid);
    passed &= expect(
        !missingUpdate && missingUpdate.error().code == ErrorCode::NotFound,
        "testing-class update did not report a missing id"
        );
    const auto invalidGet = service.get(0);
    passed &= expect(
        !invalidGet && invalidGet.error().code == ErrorCode::InvalidArgument,
        "testing-class get accepted a nonpositive id"
        );
    const auto missingGet = service.get(std::numeric_limits<int>::max());
    passed &= expect(
        !missingGet && missingGet.error().code == ErrorCode::NotFound,
        "testing-class get did not report a missing id"
        );

    if (created)
    {
        TestingClass conflicting = validTestingClass();
        conflicting.name = "Assignment rollback";
        const auto rejected = service.create(
            conflicting,
            "tuesday",
            "09:05"
            );
        passed &= expect(
            !rejected,
            "duplicate testing assignment was unexpectedly accepted"
            );
        passed &= expect(
            countEquals(
                database,
                "SELECT COUNT(*) FROM classes WHERE name=?",
                0,
                SqliteParameters{
                    SqliteValue{std::string("Assignment rollback")}
                }
                ),
            "testing-class creation did not roll back after assignment failure"
            );
    }

    int removableId = -1;
    {
        TestingClass removable = validTestingClass();
        removable.name = "Remove me";
        removable.grade = "M1";
        removable.level = "Major";
        removable.room = "Room delete";
        removable.notes = "remove notes";
        const auto createdRemovable = service.create(
            removable,
            "Monday",
            "10:10"
            );
        passed &= expect(
            createdRemovable.has_value(),
            "testing-class deletion fixture could not be created"
            );
        if (createdRemovable)
        {
            removableId = *createdRemovable;
            const auto evaluationInserted = database.execute(
                "INSERT INTO speaking_evaluations "
                "(class_id, evaluation_name) VALUES (?, ?)",
                SqliteParameters{
                    SqliteValue{std::int64_t{removableId}},
                    SqliteValue{std::string("Oral")}
                }
                );
            const std::int64_t evaluationId = lastInsertId(database);
            passed &= expect(
                evaluationInserted.has_value() && evaluationId > 0,
                "speaking-evaluation deletion fixture could not be created"
                );
            passed &= expect(
                database.execute(
                    "INSERT INTO roster_columns "
                    "(class_id, name, position, width) VALUES (?, ?, ?, ?)",
                    SqliteParameters{
                        SqliteValue{std::int64_t{removableId}},
                        SqliteValue{std::string("Column")},
                        SqliteValue{std::int64_t{0}},
                        SqliteValue{std::int64_t{100}}
                    }
                    ).has_value(),
                "roster-column deletion fixture could not be created"
                );
            passed &= expect(
                database.execute(
                    "INSERT INTO roster_data "
                    "(class_id, row_index, col_index, value) VALUES (?, ?, ?, ?)",
                    SqliteParameters{
                        SqliteValue{std::int64_t{removableId}},
                        SqliteValue{std::int64_t{0}},
                        SqliteValue{std::int64_t{0}},
                        SqliteValue{std::string("value")}
                    }
                    ).has_value(),
                "roster-data deletion fixture could not be created"
                );
            passed &= expect(
                evaluationId > 0
                    && database.execute(
                        "INSERT INTO speaking_eval_data "
                        "(evaluation_id, row_index, col_0) VALUES (?, ?, ?)",
                        SqliteParameters{
                            SqliteValue{evaluationId},
                            SqliteValue{std::int64_t{0}},
                            SqliteValue{std::string("speech")}
                        }
                        ).has_value(),
                "speaking-evaluation data deletion fixture could not be created"
                );
            passed &= expect(
                database.execute(
                    "INSERT INTO class_times "
                    "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)",
                    SqliteParameters{
                        SqliteValue{std::int64_t{removableId}},
                        SqliteValue{std::string("Monday")},
                        SqliteValue{std::string("10:00")},
                        SqliteValue{std::string("10:50")}
                    }
                    ).has_value(),
                "class-time deletion fixture could not be created"
                );
            passed &= expect(
                database.execute(
                    "INSERT INTO class_intensive_times "
                    "(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)",
                    SqliteParameters{
                        SqliteValue{std::int64_t{removableId}},
                        SqliteValue{std::string("Monday")},
                        SqliteValue{std::string("11:00")},
                        SqliteValue{std::string("11:50")}
                    }
                    ).has_value(),
                "intensive-time deletion fixture could not be created"
                );

            const auto removed = service.remove(removableId);
            passed &= expect(
                removed.has_value(),
                "testing-class deletion failed"
                );
            for (const std::string_view table : {
                     "schedule_testing_blocks",
                     "roster_columns",
                     "roster_data",
                     "speaking_evaluations",
                     "speaking_eval_data",
                     "class_times",
                     "class_intensive_times",
                     "class_info",
                     "testing_classes",
                     "classes"})
            {
                passed &= expect(
                    !hasId(database, table, table == "classes" ? "id" : "class_id", removableId),
                    "testing-class deletion left dependent rows"
                    );
            }
            const auto removedMembership = service.isTestingClass(removableId);
            passed &= expect(
                removedMembership && !*removedMembership,
                "deleted testing class still reported as a member"
                );
        }
    }

    const auto rollbackCreated = [&] {
        TestingClass rollback = validTestingClass();
        rollback.name = "Delete rollback";
        rollback.grade = "M1";
        rollback.level = "Solis";
        rollback.room = "Rollback room";
        return service.create(rollback, "Wednesday", "11:15");
    }();
    if (rollbackCreated)
    {
        const std::string triggerSql =
            "CREATE TRIGGER reject_testing_class_info_delete "
            "BEFORE DELETE ON class_info WHEN OLD.class_id = "
            + std::to_string(*rollbackCreated)
            + " BEGIN SELECT RAISE(ABORT, 'injected testing delete failure'); "
              "END";
        passed &= expect(
            database.execute(triggerSql).has_value(),
            "testing-class rollback trigger could not be created"
            );

        const auto rejected = service.remove(*rollbackCreated);
        passed &= expect(
            !rejected,
            "testing-class deletion failure fixture unexpectedly succeeded"
            );
        passed &= expect(
            hasId(database, "classes", "id", *rollbackCreated)
                && hasId(database, "testing_classes", "class_id", *rollbackCreated)
                && hasId(database, "class_info", "class_id", *rollbackCreated)
                && hasId(
                    database,
                    "schedule_testing_blocks",
                    "class_id",
                    *rollbackCreated
                    ),
            "testing-class deletion did not roll back all prior deletes"
            );
        passed &= expect(
            database.execute(
                "DROP TRIGGER reject_testing_class_info_delete"
                ).has_value(),
            "testing-class rollback trigger could not be removed"
            );
        passed &= expect(
            service.remove(*rollbackCreated).has_value(),
            "testing-class deletion after rollback failed"
            );
    }
    else
    {
        passed &= expect(
            false,
            "testing-class rollback fixture could not be created"
            );
    }

    if (removableId > 0)
    {
        const auto missingRemove = service.remove(removableId);
        passed &= expect(
            !missingRemove && missingRemove.error().code == ErrorCode::NotFound,
            "testing-class remove did not report a missing id"
            );
    }
    if (regularId > 0 && regularId <= std::numeric_limits<int>::max())
    {
        const auto regularRemove = service.remove(static_cast<int>(regularId));
        passed &= expect(
            !regularRemove && regularRemove.error().code == ErrorCode::NotFound,
            "testing-class remove treated a regular class as testing"
            );
    }

    const auto malformedCreated = [&] {
        TestingClass malformed = validTestingClass();
        malformed.name = "Malformed row";
        malformed.grade = "M1";
        malformed.level = "Song's";
        malformed.room = "Malformed room";
        return service.create(malformed);
    }();
    if (malformedCreated)
    {
        passed &= expect(
            database.execute(
                "UPDATE class_info SET class_color=? WHERE class_id=?",
                SqliteParameters{
                    SqliteValue{std::vector<std::byte>{std::byte{0x01}}},
                    SqliteValue{std::int64_t{*malformedCreated}}
                }
                ).has_value(),
            "malformed testing-class row fixture could not be written"
            );
        const auto malformedGet = service.get(*malformedCreated);
        passed &= expect(
            !malformedGet && malformedGet.error().code == ErrorCode::Schema,
            "malformed testing-class row did not return a typed schema error"
            );
        const auto malformedList = service.list();
        passed &= expect(
            !malformedList && malformedList.error().code == ErrorCode::Schema,
            "malformed testing-class list row did not return a typed schema "
            "error"
            );
        passed &= expect(
            service.remove(*malformedCreated).has_value(),
            "malformed testing-class fixture could not be cleaned up"
            );
    }
    else
    {
        passed &= expect(
            false,
            "malformed testing-class fixture could not be created"
            );
    }

    return passed ? 0 : 1;
}

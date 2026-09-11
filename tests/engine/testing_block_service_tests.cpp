#include "classmngr/engine/open_database.h"
#include "classmngr/engine/testing_block_service.h"
#include "classmngr/engine/testing_class_service.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::SqliteDatabase;
using classmngr::engine::SqliteParameters;
using classmngr::engine::SqliteValue;
using classmngr::engine::TestingAssignmentKind;
using classmngr::engine::TestingBlockService;
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

    std::cerr << "ClassMngrEngineTestingBlockServiceTests: "
              << message
              << '\n';
    return false;
}

TestingClass validTestingClass(
    std::string name,
    std::string room
    )
{
    TestingClass testingClass;
    testingClass.name = std::move(name);
    testingClass.grade = "M1";
    testingClass.level = "Mixed (All)";
    testingClass.room = std::move(room);
    testingClass.notes = "testing block fixture";
    return testingClass;
}

int createTestingClass(
    TestingClassService& service,
    std::string name,
    std::string room
    )
{
    const auto created = service.create(
        validTestingClass(std::move(name), std::move(room))
        );
    return created ? *created : -1;
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

bool countRows(
    SqliteDatabase& database,
    std::int64_t expected
    )
{
    const auto result = database.query(
        "SELECT COUNT(*) FROM schedule_testing_blocks"
        );
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
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineTestingBlockServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    TestingBlockService service(database);
    TestingClassService testingClassService(database);
    bool passed = true;

    for (const auto& invalidKey : {
             std::pair<std::string_view, std::string_view>{"Funday", "09:00"},
             std::pair<std::string_view, std::string_view>{"Monday", "9:00"},
             std::pair<std::string_view, std::string_view>{"Monday", "24:00"},
             std::pair<std::string_view, std::string_view>{"Monday", "09:60"},
             std::pair<std::string_view, std::string_view>{"Monday", "09:00x"},
             std::pair<std::string_view, std::string_view>{"", "09:00"}
         })
    {
        const auto rejected = service.saveBlock(
            invalidKey.first,
            invalidKey.second,
            "Room"
            );
        passed &= expect(
            !rejected && rejected.error().code == ErrorCode::InvalidArgument,
            "invalid testing-block key was accepted"
            );
    }

    passed &= expect(
        service.saveBlock(
            " \t tUeSdAy \r\n",
            "\n 09:05 \t",
            " \t시험실 🍜 \r\n"
            ).has_value(),
        "canonical UTF-8 testing block could not be saved"
        );

    const auto firstAssignments = service.listAssignments();
    passed &= expect(
        firstAssignments
            && firstAssignments->size() == 1
            && firstAssignments->front().day == "Tuesday"
            && firstAssignments->front().startTime == "09:05"
            && firstAssignments->front().room == "시험실 🍜"
            && firstAssignments->front().classId == -1
            && firstAssignments->front().kind
                == TestingAssignmentKind::PlainTesting,
        "testing-block normalization or UTF-8 round trip changed"
        );

    const int firstClassId = createTestingClass(
        testingClassService,
        "First testing class",
        "Classroom 1"
        );
    const int secondClassId = createTestingClass(
        testingClassService,
        "Second testing class",
        "Classroom 2"
        );
    passed &= expect(
        firstClassId > 0 && secondClassId > 0,
        "testing-class fixtures could not be created"
        );

    if (firstClassId > 0 && secondClassId > 0)
    {
        passed &= expect(
            service.assignClass(
                " Monday ",
                " 10:10 ",
                firstClassId
                ).has_value(),
            "testing class assignment failed"
            );

        const auto assignmentsWithClass = service.listAssignments();
        const auto blocksWithClass = service.listBlocks();
        passed &= expect(
            assignmentsWithClass
                && assignmentsWithClass->size() == 2
                && assignmentsWithClass->at(0).day == "Monday"
                && assignmentsWithClass->at(0).startTime == "10:10"
                && assignmentsWithClass->at(0).kind
                    == TestingAssignmentKind::SpecialClass
                && assignmentsWithClass->at(0).classId == firstClassId
                && assignmentsWithClass->at(1).day == "Tuesday"
                && blocksWithClass
                && blocksWithClass->size() == 1
                && blocksWithClass->front().day == "Tuesday"
                && blocksWithClass->front().startTime == "09:05",
            "assignment ordering or plain-block filtering changed"
            );

        passed &= expect(
            service.assignClass("Monday", "10:10", firstClassId).has_value(),
            "reassigning the same testing class was rejected"
            );

        const auto plainConflict = service.saveBlock(
            "mOnDaY",
            "10:10",
            "Replacement room"
            );
        passed &= expect(
            !plainConflict
                && plainConflict.error().code == ErrorCode::Constraint,
            "plain-block save did not reject an assigned testing class"
            );
        passed &= expect(
            service.saveBlock(
                "mOnDaY",
                "10:10",
                " Replacement room ",
                true
                ).has_value(),
            "plain-block replacement failed"
            );

        const auto replacedWithPlain = service.listAssignments();
        passed &= expect(
            replacedWithPlain
                && replacedWithPlain->size() == 2
                && replacedWithPlain->at(0).kind
                    == TestingAssignmentKind::PlainTesting
                && replacedWithPlain->at(0).classId == -1
                && replacedWithPlain->at(0).room == "Replacement room",
            "plain-block replacement did not clear the testing class"
            );

        const auto classOverPlainConflict = service.assignClass(
            "Monday",
            "10:10",
            firstClassId
            );
        passed &= expect(
            !classOverPlainConflict
                && classOverPlainConflict.error().code
                    == ErrorCode::Constraint,
            "class assignment over a plain block did not require replacement"
            );
        const auto differentClassConflict = service.assignClass(
            "Monday",
            "10:10",
            secondClassId
            );
        passed &= expect(
            !differentClassConflict
                && differentClassConflict.error().code
                    == ErrorCode::Constraint,
            "different testing class assignment did not require replacement"
            );
        passed &= expect(
            service.assignClass(
                "Monday",
                "10:10",
                secondClassId,
                true
                ).has_value(),
            "testing-class replacement failed"
            );

        const auto replacedWithClass = service.listAssignments();
        passed &= expect(
            replacedWithClass
                && replacedWithClass->at(0).kind
                    == TestingAssignmentKind::SpecialClass
                && replacedWithClass->at(0).classId == secondClassId
                && replacedWithClass->at(0).room.empty(),
            "testing-class replacement did not persist class id and room"
            );

        const auto invalidClassKey = service.assignClass(
            "Monday",
            "9:00",
            secondClassId
            );
        passed &= expect(
            !invalidClassKey
                && invalidClassKey.error().code == ErrorCode::InvalidArgument,
            "class assignment accepted a partial time key"
            );
        const auto invalidClassId = service.assignClass(
            "Monday",
            "11:00",
            0
            );
        passed &= expect(
            !invalidClassId
                && invalidClassId.error().code == ErrorCode::InvalidArgument,
            "class assignment accepted a nonpositive class id"
            );

        const auto missingClass = service.assignClass(
            "Wednesday",
            "11:00",
            std::numeric_limits<int>::max()
            );
        passed &= expect(
            !missingClass && missingClass.error().code == ErrorCode::NotFound,
            "missing testing class did not return a typed not-found error"
            );

        const int incompleteClassId = createTestingClass(
            testingClassService,
            "Incomplete testing class",
            "Classroom 3"
            );
        passed &= expect(
            incompleteClassId > 0
                && database.execute(
                    "UPDATE class_info SET class_grade=? WHERE class_id=?",
                    SqliteParameters{
                        SqliteValue{std::string(" \t")},
                        SqliteValue{std::int64_t{incompleteClassId}}
                    }
                    ).has_value(),
            "incomplete testing-class fixture could not be prepared"
            );
        const auto incompleteClassAssignment = service.assignClass(
            "Wednesday",
            "11:00",
            incompleteClassId
            );
        passed &= expect(
            !incompleteClassAssignment
                && incompleteClassAssignment.error().code
                    == ErrorCode::InvalidArgument
                && countRows(database, 2),
            "incomplete testing class was assigned or changed the schedule"
            );

        passed &= expect(
            database.execute(
                "INSERT INTO classes (name) VALUES (?)",
                SqliteParameters{
                    SqliteValue{std::string("Regular class")}
                }
                ).has_value(),
            "regular-class fixture could not be created"
            );
        const int regularClassId = static_cast<int>(lastInsertId(database));
        const auto regularClassAssignment = service.assignClass(
            "Thursday",
            "11:00",
            regularClassId
            );
        passed &= expect(
            !regularClassAssignment
                && regularClassAssignment.error().code == ErrorCode::NotFound,
            "regular class was accepted as a testing class"
            );

        passed &= expect(
            service.saveBlock(
                "Wednesday",
                "11:00",
                "Plain Wednesday"
                ).has_value(),
            "second plain testing block could not be saved"
            );
        const auto filteredBlocks = service.listBlocks();
        passed &= expect(
            filteredBlocks
                && filteredBlocks->size() == 2
                && filteredBlocks->at(0).day == "Tuesday"
                && filteredBlocks->at(0).room == "시험실 🍜"
                && filteredBlocks->at(1).day == "Wednesday"
                && filteredBlocks->at(1).room == "Plain Wednesday",
            "plain-block filtering or ordering changed"
            );

        passed &= expect(
            service.deleteAssignment(" tUeSdAy ", " 09:05 ").has_value(),
            "delete-assignment alias failed"
            );
        const auto afterAssignmentDelete = service.listAssignments();
        passed &= expect(
            afterAssignmentDelete
                && afterAssignmentDelete->size() == 2
                && afterAssignmentDelete->at(0).day == "Monday"
                && afterAssignmentDelete->at(1).day == "Wednesday",
            "delete-assignment did not remove the canonical slot"
            );
        const auto invalidDelete = service.deleteBlock("Monday", "9:00");
        passed &= expect(
            !invalidDelete
                && invalidDelete.error().code == ErrorCode::InvalidArgument,
            "delete-block accepted a partial time key"
            );
        passed &= expect(
            service.deleteBlock(" Monday ", " 10:10 ").has_value(),
            "delete-block failed"
            );
        passed &= expect(
            service.clearBlocks().has_value() && countRows(database, 0),
            "clear-blocks did not clear the assignment table"
            );

        passed &= expect(
            service.saveBlock("Friday", "12:00", "Clear alias").has_value()
                && service.clearAssignments().has_value()
                && countRows(database, 0),
            "clear-assignment alias did not clear the assignment table"
            );

        const int rollbackClassId = createTestingClass(
            testingClassService,
            "Rollback testing class",
            "Classroom 4"
            );
        passed &= expect(
            rollbackClassId > 0
                && database.execute(
                    "CREATE TRIGGER reject_testing_assignment "
                    "BEFORE INSERT ON schedule_testing_blocks "
                    "WHEN NEW.class_id=" + std::to_string(rollbackClassId) + " "
                    "BEGIN SELECT RAISE(ABORT, 'injected assignment failure'); "
                    "END"
                    ).has_value(),
            "assignment rollback trigger could not be created"
            );
        const auto assignmentFailure = service.assignClass(
            "Friday",
            "12:00",
            rollbackClassId
            );
        passed &= expect(
            !assignmentFailure
                && assignmentFailure.error().code == ErrorCode::Database
                && countRows(database, 0),
            "failed assignment did not return a typed error or roll back"
            );
        passed &= expect(
            database.execute(
                "DROP TRIGGER reject_testing_assignment"
                ).has_value()
                && service.assignClass(
                    "Friday",
                    "12:00",
                    rollbackClassId
                    ).has_value(),
            "database was not reusable after assignment rollback"
            );
    }

    const auto schemaOpened = OpenDatabase::execute(":memory:");
    passed &= expect(
        schemaOpened && *schemaOpened != nullptr,
        "OpenDatabase failed for schema-type fixture"
        );
    if (schemaOpened && *schemaOpened != nullptr)
    {
        TestingBlockService schemaService(**schemaOpened);
        passed &= expect(
            (*schemaOpened)->execute(
                "INSERT INTO schedule_testing_blocks "
                "(day, start_time, room, class_id) "
                "VALUES ('Monday', '09:00', CAST('room' AS BLOB), NULL)"
                ).has_value(),
            "schema-type fixture could not be inserted"
            );
        const auto wrongType = schemaService.listAssignments();
        passed &= expect(
            !wrongType && wrongType.error().code == ErrorCode::Schema,
            "wrong testing-assignment value type was not rejected"
            );
    }

    return passed ? 0 : 1;
}

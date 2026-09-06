#include "classmngr/engine/testing_class_service.h"

#include "classmngr/engine/sqlite_database.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace classmngr::engine
{
namespace
{
Error error(
    ErrorCode code,
    std::string message
    )
{
    return {code, std::move(message), std::nullopt};
}

Error withContext(
    Error source,
    std::string_view action,
    std::optional<int> classId = std::nullopt
    )
{
    std::string message(action);
    if (classId)
    {
        message += " for class id ";
        message += std::to_string(*classId);
    }
    if (!source.message.empty())
    {
        message += ": ";
        message += source.message;
    }
    source.message = std::move(message);
    return source;
}

bool isAsciiWhitespace(
    char value
    ) noexcept
{
    switch (value)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
        return true;
    default:
        return false;
    }
}

bool isBlank(
    std::string_view value
    ) noexcept
{
    for (const char character : value)
    {
        if (!isAsciiWhitespace(character))
        {
            return false;
        }
    }
    return true;
}

std::string trimAsciiWhitespace(
    std::string_view value
    )
{
    std::size_t first = 0;
    while (first < value.size() && isAsciiWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isAsciiWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

char asciiLower(
    char value
    ) noexcept
{
    const unsigned char character = static_cast<unsigned char>(value);
    if (character >= static_cast<unsigned char>('A')
        && character <= static_cast<unsigned char>('Z'))
    {
        return static_cast<char>(character + ('a' - 'A'));
    }
    return value;
}

bool equalsAsciiCaseInsensitive(
    std::string_view left,
    std::string_view right
    ) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (asciiLower(left[index]) != asciiLower(right[index]))
        {
            return false;
        }
    }

    return true;
}

Result<std::string> canonicalWeekday(
    std::string_view value
    )
{
    static constexpr std::array<std::string_view, 7> weekdays{
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
        "Sunday"
    };

    const std::string normalized = trimAsciiWhitespace(value);
    for (const std::string_view weekday : weekdays)
    {
        if (equalsAsciiCaseInsensitive(normalized, weekday))
        {
            return std::string(weekday);
        }
    }

    return std::unexpected(error(
        ErrorCode::InvalidArgument,
        "Testing class assignment day must be Monday through Sunday."
        ));
}

Result<std::string> canonicalTime(
    std::string_view value
    )
{
    const std::string normalized = trimAsciiWhitespace(value);
    if (normalized.size() != 5
        || normalized[2] != ':'
        || !std::isdigit(static_cast<unsigned char>(normalized[0]))
        || !std::isdigit(static_cast<unsigned char>(normalized[1]))
        || !std::isdigit(static_cast<unsigned char>(normalized[3]))
        || !std::isdigit(static_cast<unsigned char>(normalized[4])))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Testing class assignment time must use strict HH:mm format."
            ));
    }

    const int hour = (normalized[0] - '0') * 10 + (normalized[1] - '0');
    const int minute = (normalized[3] - '0') * 10 + (normalized[4] - '0');
    if (hour > 23 || minute > 59)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Testing class assignment time must use strict HH:mm format."
            ));
    }

    return normalized;
}

Result<TestingClass> normalizedAndValidated(
    const TestingClass& source,
    bool requireId
    )
{
    if (requireId && source.classId <= 0)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Updating a testing class requires a positive class id."
            ));
    }

    TestingClass normalized = source;
    normalized.name = trimAsciiWhitespace(source.name);
    normalized.grade = trimAsciiWhitespace(source.grade);
    normalized.level = trimAsciiWhitespace(source.level);
    normalized.room = trimAsciiWhitespace(source.room);
    normalized.classColor = trimAsciiWhitespace(source.classColor);
    normalized.fontColor = trimAsciiWhitespace(source.fontColor);

    if (normalized.name.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Testing class name is required."
            ));
    }
    if (normalized.grade.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Testing class grade is required."
            ));
    }
    if (normalized.level.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Testing class level is required."
            ));
    }
    if (normalized.room.empty())
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Testing class room is required."
            ));
    }

    return normalized;
}

Result<int> positiveIntValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    const auto* integer = std::get_if<std::int64_t>(&value);
    if (integer == nullptr
        || *integer <= 0
        || *integer > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid testing class "
                + std::string(column) + "."
            ));
    }

    return static_cast<int>(*integer);
}

Result<int> teacherIdValue(
    const SqliteValue& value
    )
{
    if (std::holds_alternative<std::monostate>(value))
    {
        return -1;
    }

    const auto* integer = std::get_if<std::int64_t>(&value);
    if (integer == nullptr
        || *integer < std::numeric_limits<int>::min()
        || *integer > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid testing class teacher_id."
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
        "SQLite returned a non-text testing class "
            + std::string(column) + "."
        ));
}

Result<TestingClass> testingClassFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 9)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected testing class row shape."
            ));
    }

    const Result<int> classId = positiveIntValue(
        row.values[0],
        "class id"
        );
    if (!classId)
    {
        return std::unexpected(classId.error());
    }

    const Result<std::string> name = textValue(row.values[1], "name");
    const Result<std::string> room = textValue(row.values[2], "room");
    const Result<int> teacherId = teacherIdValue(row.values[3]);
    const Result<std::string> grade = textValue(
        row.values[4],
        "class_grade"
        );
    const Result<std::string> level = textValue(
        row.values[5],
        "class_level"
        );
    const Result<std::string> classColor = textValue(
        row.values[6],
        "class_color"
        );
    const Result<std::string> fontColor = textValue(
        row.values[7],
        "font_color"
        );
    const Result<std::string> notes = textValue(row.values[8], "notes");

    if (!name)
    {
        return std::unexpected(name.error());
    }
    if (!room)
    {
        return std::unexpected(room.error());
    }
    if (!teacherId)
    {
        return std::unexpected(teacherId.error());
    }
    if (!grade)
    {
        return std::unexpected(grade.error());
    }
    if (!level)
    {
        return std::unexpected(level.error());
    }
    if (!classColor)
    {
        return std::unexpected(classColor.error());
    }
    if (!fontColor)
    {
        return std::unexpected(fontColor.error());
    }
    if (!notes)
    {
        return std::unexpected(notes.error());
    }

    TestingClass testingClass;
    testingClass.classId = *classId;
    testingClass.name = *name;
    testingClass.grade = *grade;
    testingClass.level = *level;
    testingClass.room = *room;
    testingClass.teacherId = *teacherId;
    testingClass.classColor = *classColor;
    testingClass.fontColor = *fontColor;
    testingClass.notes = *notes;

    if (isBlank(testingClass.classColor))
    {
        testingClass.classColor = "#FFFFFF";
    }
    if (isBlank(testingClass.fontColor))
    {
        testingClass.fontColor = "#000000";
    }

    return testingClass;
}

const char* testingClassSelect()
{
    return "SELECT c.id AS class_id, c.name, tc.room, ci.teacher_id, "
        "ci.class_grade, ci.class_level, ci.class_color, ci.font_color, "
        "ci.notes FROM testing_classes tc "
        "JOIN classes c ON c.id=tc.class_id "
        "LEFT JOIN class_info ci ON ci.class_id=tc.class_id";
}

Result<bool> testingClassExists(
    SqliteDatabase& database,
    int classId
    )
{
    const auto rows = database.query(
        "SELECT EXISTS(SELECT 1 FROM testing_classes WHERE class_id=?)",
        SqliteParameters{
            SqliteValue{std::int64_t{classId}}
        }
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }
    if (rows->rows.size() != 1 || rows->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected testing class existence result."
            ));
    }

    const auto* value = std::get_if<std::int64_t>(
        &rows->rows.front().values.front()
        );
    if (value == nullptr)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned a non-integer testing class existence result."
            ));
    }

    return *value != 0;
}

Status validClassId(
    int classId,
    std::string_view action
    )
{
    if (classId > 0)
    {
        return {};
    }

    return std::unexpected(error(
        ErrorCode::InvalidArgument,
        std::string(action) + " requires a positive class id."
        ));
}

Error notFound(
    int classId
    )
{
    return error(
        ErrorCode::NotFound,
        "No testing class exists for id " + std::to_string(classId) + "."
        );
}

Status deleteRows(
    SqliteDatabase& database,
    int classId,
    std::string_view sql,
    std::string_view action
    )
{
    const Status deleted = database.execute(
        sql,
        SqliteParameters{
            SqliteValue{std::int64_t{classId}}
        }
        );
    if (!deleted)
    {
        return std::unexpected(withContext(
            deleted.error(),
            action,
            classId
            ));
    }

    return {};
}
} // namespace

TestingClassService::TestingClassService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<int> TestingClassService::create(
    const TestingClass& testingClass,
    std::string_view assignmentDay,
    std::string_view assignmentStartTime
    )
{
    const Result<TestingClass> normalized = normalizedAndValidated(
        testingClass,
        false
        );
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }

    const std::string trimmedAssignmentDay = trimAsciiWhitespace(
        assignmentDay
        );
    const std::string trimmedAssignmentTime = trimAsciiWhitespace(
        assignmentStartTime
        );
    const bool hasAssignment = !trimmedAssignmentDay.empty()
        || !trimmedAssignmentTime.empty();

    std::string canonicalDay;
    std::string canonicalStartTime;
    if (hasAssignment)
    {
        const Result<std::string> parsedDay = canonicalWeekday(
            trimmedAssignmentDay
            );
        const Result<std::string> parsedTime = canonicalTime(
            trimmedAssignmentTime
            );
        if (!parsedDay || !parsedTime)
        {
            return std::unexpected(error(
                ErrorCode::InvalidArgument,
                "A testing assignment requires a valid weekday and start "
                "time."
                ));
        }
        canonicalDay = *parsedDay;
        canonicalStartTime = *parsedTime;
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting testing class creation transaction"
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Status classInserted = m_database.execute(
        "INSERT INTO classes (name) VALUES (?)",
        SqliteParameters{
            SqliteValue{normalized->name}
        }
        );
    if (!classInserted)
    {
        return std::unexpected(withContext(
            classInserted.error(),
            "Creating testing class"
            ));
    }

    const auto rowId = m_database.query("SELECT last_insert_rowid()");
    if (!rowId)
    {
        return std::unexpected(withContext(
            rowId.error(),
            "Reading created testing class id"
            ));
    }
    if (rowId->rows.size() != 1 || rowId->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite did not return the new testing class id."
            ));
    }
    const Result<int> classId = positiveIntValue(
        rowId->rows.front().values.front(),
        "id"
        );
    if (!classId)
    {
        return std::unexpected(classId.error());
    }

    const SqliteValue teacherId = normalized->teacherId > 0
        ? SqliteValue{std::int64_t{normalized->teacherId}}
        : SqliteValue{std::monostate{}};
    const Status infoInserted = m_database.execute(
        "INSERT INTO class_info ("
        "class_id, teacher_id, class_grade, class_level, class_color, "
        "font_color, notes, time_filler_activities"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, '')",
        SqliteParameters{
            SqliteValue{std::int64_t{*classId}},
            teacherId,
            SqliteValue{normalized->grade},
            SqliteValue{normalized->level},
            SqliteValue{normalized->classColor},
            SqliteValue{normalized->fontColor},
            SqliteValue{normalized->notes}
        }
        );
    if (!infoInserted)
    {
        return std::unexpected(withContext(
            infoInserted.error(),
            "Saving testing class details",
            *classId
            ));
    }

    const Status testingClassInserted = m_database.execute(
        "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)",
        SqliteParameters{
            SqliteValue{std::int64_t{*classId}},
            SqliteValue{normalized->room}
        }
        );
    if (!testingClassInserted)
    {
        return std::unexpected(withContext(
            testingClassInserted.error(),
            "Saving the testing class room",
            *classId
            ));
    }

    if (hasAssignment)
    {
        const Status assignmentInserted = m_database.execute(
            "INSERT INTO schedule_testing_blocks ("
            "day, start_time, room, class_id"
            ") VALUES (?, ?, '', ?)",
            SqliteParameters{
                SqliteValue{canonicalDay},
                SqliteValue{canonicalStartTime},
                SqliteValue{std::int64_t{*classId}}
            }
            );
        if (!assignmentInserted)
        {
            return std::unexpected(withContext(
                assignmentInserted.error(),
                "Assigning the new testing class",
                *classId
                ));
        }
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing testing class creation",
            *classId
            ));
    }

    return *classId;
}

Status TestingClassService::update(
    const TestingClass& testingClass
    )
{
    const Result<TestingClass> normalized = normalizedAndValidated(
        testingClass,
        true
        );
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting testing class update transaction",
            testingClass.classId
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Result<bool> exists = testingClassExists(
        m_database,
        testingClass.classId
        );
    if (!exists)
    {
        return std::unexpected(withContext(
            exists.error(),
            "Checking the testing class",
            testingClass.classId
            ));
    }
    if (!*exists)
    {
        return std::unexpected(notFound(testingClass.classId));
    }

    const Status classUpdated = m_database.execute(
        "UPDATE classes SET name=? WHERE id=?",
        SqliteParameters{
            SqliteValue{normalized->name},
            SqliteValue{std::int64_t{testingClass.classId}}
        }
        );
    if (!classUpdated)
    {
        return std::unexpected(withContext(
            classUpdated.error(),
            "Updating the testing class name",
            testingClass.classId
            ));
    }

    const SqliteValue teacherId = normalized->teacherId > 0
        ? SqliteValue{std::int64_t{normalized->teacherId}}
        : SqliteValue{std::monostate{}};
    const Status infoUpdated = m_database.execute(
        "INSERT INTO class_info ("
        "class_id, teacher_id, class_grade, class_level, class_color, "
        "font_color, notes, time_filler_activities"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, '') "
        "ON CONFLICT(class_id) DO UPDATE SET "
        "teacher_id=excluded.teacher_id, "
        "class_grade=excluded.class_grade, "
        "class_level=excluded.class_level, "
        "class_color=excluded.class_color, "
        "font_color=excluded.font_color, "
        "notes=excluded.notes",
        SqliteParameters{
            SqliteValue{std::int64_t{testingClass.classId}},
            teacherId,
            SqliteValue{normalized->grade},
            SqliteValue{normalized->level},
            SqliteValue{normalized->classColor},
            SqliteValue{normalized->fontColor},
            SqliteValue{normalized->notes}
        }
        );
    if (!infoUpdated)
    {
        return std::unexpected(withContext(
            infoUpdated.error(),
            "Updating testing class details",
            testingClass.classId
            ));
    }

    const Status roomUpdated = m_database.execute(
        "UPDATE testing_classes SET room=? WHERE class_id=?",
        SqliteParameters{
            SqliteValue{normalized->room},
            SqliteValue{std::int64_t{testingClass.classId}}
        }
        );
    if (!roomUpdated)
    {
        return std::unexpected(withContext(
            roomUpdated.error(),
            "Updating the testing class room",
            testingClass.classId
            ));
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing testing class update",
            testingClass.classId
            ));
    }

    return {};
}

Result<TestingClass> TestingClassService::get(
    int classId
    )
{
    const Status valid = validClassId(classId, "Loading a testing class");
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    const auto rows = m_database.query(
        std::string(testingClassSelect()) + " WHERE tc.class_id=?",
        SqliteParameters{
            SqliteValue{std::int64_t{classId}}
        }
        );
    if (!rows)
    {
        return std::unexpected(withContext(
            rows.error(),
            "Loading the testing class",
            classId
            ));
    }
    if (rows->rows.empty())
    {
        return std::unexpected(notFound(classId));
    }

    const Result<TestingClass> testingClass = testingClassFromRow(
        rows->rows.front()
        );
    if (!testingClass)
    {
        return std::unexpected(withContext(
            testingClass.error(),
            "Loading the testing class",
            classId
            ));
    }

    return *testingClass;
}

Result<std::vector<TestingClass>> TestingClassService::list()
{
    const auto rows = m_database.query(
        std::string(testingClassSelect())
            + " ORDER BY ci.class_grade, ci.class_level, c.name, c.id"
        );
    if (!rows)
    {
        return std::unexpected(withContext(
            rows.error(),
            "Loading testing classes"
            ));
    }

    std::vector<TestingClass> testingClasses;
    testingClasses.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<TestingClass> testingClass = testingClassFromRow(row);
        if (!testingClass)
        {
            return std::unexpected(withContext(
                testingClass.error(),
                "Loading testing classes"
                ));
        }
        testingClasses.push_back(*testingClass);
    }

    return testingClasses;
}

Status TestingClassService::remove(
    int classId
    )
{
    const Status valid = validClassId(classId, "Deleting a testing class");
    if (!valid)
    {
        return valid;
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting testing class deletion transaction",
            classId
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    const Result<bool> exists = testingClassExists(m_database, classId);
    if (!exists)
    {
        return std::unexpected(withContext(
            exists.error(),
            "Checking the testing class",
            classId
            ));
    }
    if (!*exists)
    {
        return std::unexpected(notFound(classId));
    }

    for (const auto& [sql, action] : {
             std::pair{
                 std::string_view{
                     "DELETE FROM schedule_testing_blocks WHERE class_id=?"
                     },
                 std::string_view{"Removing testing assignments"}},
             std::pair{
                 std::string_view{"DELETE FROM roster_columns WHERE class_id=?"},
                 std::string_view{"Removing roster columns"}},
             std::pair{
                 std::string_view{"DELETE FROM roster_data WHERE class_id=?"},
                 std::string_view{"Removing roster data"}},
             std::pair{
                 std::string_view{
                     "DELETE FROM speaking_eval_data WHERE evaluation_id IN ("
                     "SELECT id FROM speaking_evaluations WHERE class_id=?"
                     ")"
                     },
                 std::string_view{"Removing speaking evaluation data"}},
             std::pair{
                 std::string_view{
                     "DELETE FROM speaking_evaluations WHERE class_id=?"
                     },
                 std::string_view{"Removing speaking evaluations"}},
             std::pair{
                 std::string_view{"DELETE FROM class_times WHERE class_id=?"},
                 std::string_view{"Removing regular class times"}},
             std::pair{
                 std::string_view{
                     "DELETE FROM class_intensive_times WHERE class_id=?"
                     },
                 std::string_view{"Removing intensive class times"}},
             std::pair{
                 std::string_view{"DELETE FROM class_info WHERE class_id=?"},
                 std::string_view{"Removing class details"}},
             std::pair{
                 std::string_view{"DELETE FROM testing_classes WHERE class_id=?"},
                 std::string_view{"Removing the testing class profile"}},
             std::pair{
                 std::string_view{"DELETE FROM classes WHERE id=?"},
                 std::string_view{"Removing the testing class"}}})
    {
        const Status deleted = deleteRows(
            m_database,
            classId,
            sql,
            action
            );
        if (!deleted)
        {
            return deleted;
        }
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing testing class deletion",
            classId
            ));
    }

    return {};
}

Result<bool> TestingClassService::isTestingClass(
    int classId
    )
{
    if (classId <= 0)
    {
        return false;
    }

    const Result<bool> exists = testingClassExists(m_database, classId);
    if (!exists)
    {
        return std::unexpected(withContext(
            exists.error(),
            "Checking the testing class",
            classId
            ));
    }

    return *exists;
}

std::vector<std::string> testingClassMixedLevels()
{
    return {
        "Mixed (All)",
        "Mixed (High)",
        "Mixed (Low)"
    };
}

std::vector<std::string> testingClassGrades()
{
    return {
        "M1",
        "M2",
        "Mixed"
    };
}

std::vector<std::string> testingClassLevelsForGrade(
    std::string_view grade
    )
{
    std::vector<std::string> levels = testingClassMixedLevels();

    if (grade == "M1")
    {
        levels.insert(
            levels.end(),
            {"Song's", "Major", "Solis", "Galaxia", "Elephantus"}
            );
    }
    else if (grade == "M2")
    {
        levels.insert(
            levels.end(),
            {"Song's", "Major", "Tigris", "Leo", "Ursa"}
            );
    }

    return levels;
}

} // namespace classmngr::engine

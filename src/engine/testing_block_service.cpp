#include "classmngr/engine/testing_block_service.h"

#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/testing_class_service.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace classmngr::engine
{
namespace
{
struct CanonicalScheduleKey
{
    std::string day;
    std::string startTime;
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
    std::string_view action
    )
{
    if (source.message.empty())
    {
        source.message = std::string(action);
    }
    else
    {
        source.message = std::string(action) + ": " + source.message;
    }
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
        "Testing block day must be Monday through Sunday."
        ));
}

Result<std::string> canonicalTime(
    std::string_view value
    )
{
    const std::string normalized = trimAsciiWhitespace(value);
    const auto isDigit = [](char character) noexcept
    {
        return character >= '0' && character <= '9';
    };

    if (normalized.size() != 5
        || normalized[2] != ':'
        || !isDigit(normalized[0])
        || !isDigit(normalized[1])
        || !isDigit(normalized[3])
        || !isDigit(normalized[4]))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Testing block start time must use strict HH:mm format."
            ));
    }

    const int hour = (normalized[0] - '0') * 10 + (normalized[1] - '0');
    const int minute = (normalized[3] - '0') * 10 + (normalized[4] - '0');
    if (hour > 23 || minute > 59)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Testing block start time must use strict HH:mm format."
            ));
    }

    return normalized;
}

Result<CanonicalScheduleKey> canonicalScheduleKey(
    std::string_view day,
    std::string_view startTime
    )
{
    const Result<std::string> normalizedDay = canonicalWeekday(day);
    if (!normalizedDay)
    {
        return std::unexpected(normalizedDay.error());
    }

    const Result<std::string> normalizedTime = canonicalTime(startTime);
    if (!normalizedTime)
    {
        return std::unexpected(normalizedTime.error());
    }

    return CanonicalScheduleKey{
        *normalizedDay,
        *normalizedTime
    };
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

Result<std::string> textValue(
    const SqliteValue& value,
    std::string_view column
    )
{
    if (const auto* text = std::get_if<std::string>(&value); text != nullptr)
    {
        return *text;
    }

    return std::unexpected(error(
        ErrorCode::Schema,
        "SQLite returned a non-text testing assignment "
            + std::string(column) + "."
        ));
}

Result<std::optional<int>> existingClassId(
    SqliteDatabase& database,
    const CanonicalScheduleKey& key
    )
{
    const auto rows = database.query(
        "SELECT class_id FROM schedule_testing_blocks "
        "WHERE day=? AND start_time=?",
        SqliteParameters{
            SqliteValue{key.day},
            SqliteValue{key.startTime}
        }
        );
    if (!rows)
    {
        return std::unexpected(withContext(
            rows.error(),
            "Checking the testing assignment"
            ));
    }

    if (rows->rows.empty())
    {
        return std::nullopt;
    }

    if (rows->rows.front().values.size() != 1)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected testing assignment row shape."
            ));
    }

    const SqliteValue& value = rows->rows.front().values.front();
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
            "SQLite returned an invalid testing assignment class id."
            ));
    }

    return static_cast<int>(*integer);
}

Result<TestingAssignment> assignmentFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 4)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected testing assignment row shape."
            ));
    }

    const Result<std::string> day = textValue(row.values[0], "day");
    const Result<std::string> startTime = textValue(
        row.values[1],
        "start_time"
        );
    const Result<std::string> room = textValue(row.values[2], "room");
    if (!day)
    {
        return std::unexpected(day.error());
    }
    if (!startTime)
    {
        return std::unexpected(startTime.error());
    }
    if (!room)
    {
        return std::unexpected(room.error());
    }

    int classId = -1;
    if (!std::holds_alternative<std::monostate>(row.values[3]))
    {
        const auto* integer = std::get_if<std::int64_t>(&row.values[3]);
        if (integer == nullptr
            || *integer < std::numeric_limits<int>::min()
            || *integer > std::numeric_limits<int>::max())
        {
            return std::unexpected(error(
                ErrorCode::Schema,
                "SQLite returned an invalid testing assignment class id."
                ));
        }
        classId = static_cast<int>(*integer);
    }

    return TestingAssignment{
        *day,
        *startTime,
        classId > 0
            ? TestingAssignmentKind::SpecialClass
            : TestingAssignmentKind::PlainTesting,
        *room,
        classId
    };
}

Status validateTestingClass(
    const TestingClass& testingClass
    )
{
    if (isBlank(testingClass.name)
        || isBlank(testingClass.grade)
        || isBlank(testingClass.level)
        || isBlank(testingClass.room))
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "The selected testing class is missing required details."
            ));
    }

    return {};
}
} // namespace

TestingBlockService::TestingBlockService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<std::vector<TestingAssignment>> TestingBlockService::listAssignments()
{
    const auto rows = m_database.query(
        "SELECT day, start_time, room, class_id "
        "FROM schedule_testing_blocks "
        "ORDER BY day, start_time"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<TestingAssignment> assignments;
    assignments.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<TestingAssignment> assignment = assignmentFromRow(row);
        if (!assignment)
        {
            return std::unexpected(assignment.error());
        }
        assignments.push_back(*assignment);
    }

    return assignments;
}

Result<std::vector<TestingBlock>> TestingBlockService::listBlocks()
{
    const Result<std::vector<TestingAssignment>> assignments =
        listAssignments();
    if (!assignments)
    {
        return std::unexpected(assignments.error());
    }

    std::vector<TestingBlock> blocks;
    blocks.reserve(assignments->size());
    for (const TestingAssignment& assignment : *assignments)
    {
        if (assignment.kind != TestingAssignmentKind::PlainTesting)
        {
            continue;
        }

        blocks.push_back({
            assignment.day,
            assignment.startTime,
            assignment.room
        });
    }

    return blocks;
}

Status TestingBlockService::saveBlock(
    std::string_view day,
    std::string_view startTime,
    std::string_view room,
    bool replaceExisting
    )
{
    const Result<CanonicalScheduleKey> key = canonicalScheduleKey(
        day,
        startTime
        );
    if (!key)
    {
        return std::unexpected(key.error());
    }

    const Result<std::optional<int>> existing = existingClassId(
        m_database,
        *key
        );
    if (!existing)
    {
        return std::unexpected(existing.error());
    }

    if (existing->has_value() && **existing > 0 && !replaceExisting)
    {
        return std::unexpected(error(
            ErrorCode::Constraint,
            "This slot is assigned to a testing class. Confirm replacement "
            "first."
            ));
    }

    return m_database.execute(
        "INSERT INTO schedule_testing_blocks ("
        "day, start_time, room, class_id"
        ") VALUES (?, ?, ?, NULL) "
        "ON CONFLICT(day, start_time) DO UPDATE SET "
        "room=excluded.room, class_id=NULL",
        SqliteParameters{
            SqliteValue{key->day},
            SqliteValue{key->startTime},
            SqliteValue{trimAsciiWhitespace(room)}
        }
        );
}

Status TestingBlockService::assignClass(
    std::string_view day,
    std::string_view startTime,
    int classId,
    bool replaceExisting
    )
{
    const Result<CanonicalScheduleKey> key = canonicalScheduleKey(
        day,
        startTime
        );
    if (!key)
    {
        return std::unexpected(key.error());
    }
    if (classId <= 0)
    {
        return std::unexpected(error(
            ErrorCode::InvalidArgument,
            "Assigning a testing class requires a positive class id."
            ));
    }

    Result<SqliteTransaction> transactionResult =
        m_database.beginTransaction();
    if (!transactionResult)
    {
        return std::unexpected(withContext(
            transactionResult.error(),
            "Starting testing assignment transaction"
            ));
    }
    SqliteTransaction transaction = std::move(*transactionResult);

    TestingClassService testingClassService(m_database);
    const Result<TestingClass> testingClass = testingClassService.get(classId);
    if (!testingClass)
    {
        return std::unexpected(withContext(
            testingClass.error(),
            "Checking the testing class"
            ));
    }

    const Status validClass = validateTestingClass(*testingClass);
    if (!validClass)
    {
        return validClass;
    }

    const Result<std::optional<int>> existing = existingClassId(
        m_database,
        *key
        );
    if (!existing)
    {
        return std::unexpected(existing.error());
    }

    if (existing->has_value()
        && **existing != classId
        && !replaceExisting)
    {
        return std::unexpected(error(
            ErrorCode::Constraint,
            "This slot already has a testing assignment. Confirm replacement "
            "first."
            ));
    }

    const Status assigned = m_database.execute(
        "INSERT INTO schedule_testing_blocks ("
        "day, start_time, room, class_id"
        ") VALUES (?, ?, '', ?) "
        "ON CONFLICT(day, start_time) DO UPDATE SET "
        "room='', class_id=excluded.class_id",
        SqliteParameters{
            SqliteValue{key->day},
            SqliteValue{key->startTime},
            SqliteValue{std::int64_t{classId}}
        }
        );
    if (!assigned)
    {
        return std::unexpected(withContext(
            assigned.error(),
            "Assigning the testing class"
            ));
    }

    const Status committed = transaction.commit();
    if (!committed)
    {
        return std::unexpected(withContext(
            committed.error(),
            "Committing the testing assignment"
            ));
    }

    return {};
}

Status TestingBlockService::deleteAssignment(
    std::string_view day,
    std::string_view startTime
    )
{
    return deleteBlock(day, startTime);
}

Status TestingBlockService::deleteBlock(
    std::string_view day,
    std::string_view startTime
    )
{
    const Result<CanonicalScheduleKey> key = canonicalScheduleKey(
        day,
        startTime
        );
    if (!key)
    {
        return std::unexpected(key.error());
    }

    return m_database.execute(
        "DELETE FROM schedule_testing_blocks "
        "WHERE day=? AND start_time=?",
        SqliteParameters{
            SqliteValue{key->day},
            SqliteValue{key->startTime}
        }
        );
}

Status TestingBlockService::clearAssignments()
{
    return m_database.execute(
        "DELETE FROM schedule_testing_blocks"
        );
}

Status TestingBlockService::clearBlocks()
{
    return clearAssignments();
}

} // namespace classmngr::engine

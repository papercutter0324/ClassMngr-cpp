#include "classmngr/engine/native_english_teacher_service.h"

#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/validation_result.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
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
    return {
        code,
        std::move(message),
        std::nullopt
    };
}

bool isAsciiWhitespace(
    char character
    ) noexcept
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
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

std::string simplified(
    std::string_view value
    )
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;
    for (const char character : value)
    {
        if (isAsciiWhitespace(character))
        {
            pendingSpace = !result.empty();
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(character);
    }

    return trimAsciiWhitespace(result);
}

std::string lowerAscii(
    std::string value
    )
{
    for (char& character : value)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }

    return value;
}

std::string nameKey(
    std::string_view name
    )
{
    return lowerAscii(simplified(name));
}

NativeEnglishTeacher normalized(
    const NativeEnglishTeacher& source
    )
{
    NativeEnglishTeacher teacher = source;
    teacher.name = simplified(source.name);
    teacher.position = trimAsciiWhitespace(source.position);
    teacher.phoneNumber = trimAsciiWhitespace(source.phoneNumber);
    teacher.birthday = trimAsciiWhitespace(source.birthday);
    teacher.nationality = trimAsciiWhitespace(source.nationality);
    teacher.email = trimAsciiWhitespace(source.email);
    return teacher;
}

ValidationResult validateDirectory(
    const std::vector<NativeEnglishTeacher>& teachers
    )
{
    std::unordered_set<std::string> names;
    names.reserve(teachers.size());
    for (const NativeEnglishTeacher& source : teachers)
    {
        const NativeEnglishTeacher teacher = normalized(source);
        if (teacher.name.empty())
        {
            return ValidationResult(ValidationIssue{
                "native_english_teacher.name.required",
                "name",
                ValidationSeverity::Error
            });
        }

        if (!names.insert(nameKey(teacher.name)).second)
        {
            return ValidationResult(ValidationIssue{
                "native_english_teacher.name.duplicate",
                "name",
                ValidationSeverity::Error
            });
        }
    }

    return {};
}

Result<int> idFromValue(
    const SqliteValue& value
    )
{
    const auto* id = std::get_if<std::int64_t>(&value);
    if (id == nullptr
        || *id <= 0
        || *id > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an invalid native-English-teacher id."
            ));
    }

    return static_cast<int>(*id);
}

Result<std::string> textFromValue(
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
        "SQLite returned a non-text native-English-teacher "
        + std::string(column) + " value."
        ));
}

Result<NativeEnglishTeacher> teacherFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 7)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected native-English-teacher row shape."
            ));
    }

    const Result<int> id = idFromValue(row.values[0]);
    if (!id)
    {
        return std::unexpected(id.error());
    }

    NativeEnglishTeacher teacher;
    teacher.id = *id;
    std::string* const fields[] = {
        &teacher.name,
        &teacher.position,
        &teacher.phoneNumber,
        &teacher.birthday,
        &teacher.nationality,
        &teacher.email
    };
    const std::string_view columnNames[] = {
        "name",
        "position",
        "phone_number",
        "birthday",
        "nationality",
        "email"
    };

    for (std::size_t index = 0; index < 6; ++index)
    {
        const Result<std::string> value = textFromValue(
            row.values[index + 1],
            columnNames[index]
            );
        if (!value)
        {
            return std::unexpected(value.error());
        }
        *fields[index] = *value;
    }

    return teacher;
}

SqliteParameters parameters(
    const NativeEnglishTeacher& teacher
    )
{
    return {
        SqliteValue{teacher.name},
        SqliteValue{teacher.position},
        SqliteValue{teacher.phoneNumber},
        SqliteValue{teacher.birthday},
        SqliteValue{teacher.nationality},
        SqliteValue{teacher.email}
    };
}

int positionRank(
    std::string_view position
    ) noexcept
{
    static constexpr std::array<std::string_view, 8> orderedPositions{
        "Co-ordinator",
        "Team Leader",
        "M3 Song's",
        "M2 Song's",
        "M1 Song's",
        "E6 Song's",
        "E5 Athena",
        "NET"
    };
    const auto found = std::find(
        orderedPositions.begin(),
        orderedPositions.end(),
        position
        );
    return found == orderedPositions.end()
        ? 9
        : static_cast<int>(found - orderedPositions.begin()) + 1;
}
} // namespace

NativeEnglishTeacherService::NativeEnglishTeacherService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<std::vector<NativeEnglishTeacher>> NativeEnglishTeacherService::list() const
{
    const auto rows = m_database.query(
        "SELECT id, name, position, phone_number, birthday, nationality, email "
        "FROM native_english_teachers "
        "ORDER BY CASE position "
        "WHEN 'Co-ordinator' THEN 1 "
        "WHEN 'Team Leader' THEN 2 "
        "WHEN 'M3 Song''s' THEN 3 "
        "WHEN 'M2 Song''s' THEN 4 "
        "WHEN 'M1 Song''s' THEN 5 "
        "WHEN 'E6 Song''s' THEN 6 "
        "WHEN 'E5 Athena' THEN 7 "
        "WHEN 'NET' THEN 8 "
        "ELSE 9 END, name COLLATE NOCASE, id"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<NativeEnglishTeacher> teachers;
    teachers.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<NativeEnglishTeacher> teacher = teacherFromRow(row);
        if (!teacher)
        {
            return std::unexpected(teacher.error());
        }
        teachers.push_back(*teacher);
    }

    return teachers;
}

Status NativeEnglishTeacherService::saveDirectory(
    const std::vector<NativeEnglishTeacher>& teachers,
    const std::vector<int>& deletedIds
    )
{
    const ValidationResult validation = validateDirectory(teachers);
    if (validation.hasErrors())
    {
        const ValidationIssue& issue = validation.issues().front();
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "Native English Teacher directory validation failed: "
                + issue.code + "."
            ));
    }

    auto transaction = m_database.beginTransaction();
    if (!transaction)
    {
        return std::unexpected(transaction.error());
    }

    for (const int teacherId : deletedIds)
    {
        if (teacherId <= 0)
        {
            continue;
        }

        const Status deleted = m_database.execute(
            "DELETE FROM native_english_teachers WHERE id=?",
            SqliteParameters{
                SqliteValue{std::int64_t{teacherId}}
            }
            );
        if (!deleted)
        {
            return deleted;
        }
    }

    for (const NativeEnglishTeacher& source : teachers)
    {
        const NativeEnglishTeacher teacher = normalized(source);
        SqliteParameters values = parameters(teacher);
        if (teacher.id > 0)
        {
            values.push_back(SqliteValue{std::int64_t{teacher.id}});
            const Status updated = m_database.execute(
                "UPDATE native_english_teachers SET "
                "name=?, position=?, phone_number=?, birthday=?, "
                "nationality=?, email=? WHERE id=?",
                values
                );
            if (!updated)
            {
                return updated;
            }
        }
        else
        {
            const Status inserted = m_database.execute(
                "INSERT INTO native_english_teachers "
                "(name, position, phone_number, birthday, nationality, email) "
                "VALUES (?, ?, ?, ?, ?, ?)",
                values
                );
            if (!inserted)
            {
                return inserted;
            }
        }
    }

    return transaction->commit();
}

} // namespace classmngr::engine

#include "classmngr/engine/gs_team_service.h"

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

std::string key(
    std::string_view value
    )
{
    return lowerAscii(simplified(value));
}

GsTeamMember normalized(
    const GsTeamMember& source
    )
{
    GsTeamMember member = source;
    member.name = simplified(source.name);
    member.koreanName = simplified(source.koreanName);
    member.position = trimAsciiWhitespace(source.position);
    member.phoneNumber = trimAsciiWhitespace(source.phoneNumber);
    member.birthday = trimAsciiWhitespace(source.birthday);
    return member;
}

ValidationResult validateDirectory(
    const std::vector<GsTeamMember>& members
    )
{
    std::unordered_set<std::string> englishNames;
    std::unordered_set<std::string> koreanNames;
    englishNames.reserve(members.size());
    koreanNames.reserve(members.size());

    for (const GsTeamMember& source : members)
    {
        const GsTeamMember member = normalized(source);
        const std::string english = key(member.name);
        const std::string korean = key(member.koreanName);
        if (english.empty() && korean.empty())
        {
            return ValidationResult(ValidationIssue{
                "gs_team.name.required",
                "name",
                ValidationSeverity::Error
            });
        }
        if ((!english.empty() && !englishNames.insert(english).second)
            || (!korean.empty() && !koreanNames.insert(korean).second))
        {
            return ValidationResult(ValidationIssue{
                "gs_team.name.duplicate",
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
            "SQLite returned an invalid GS-team member id."
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
        "SQLite returned a non-text GS-team "
        + std::string(column) + " value."
        ));
}

Result<GsTeamMember> memberFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 6)
    {
        return std::unexpected(error(
            ErrorCode::Schema,
            "SQLite returned an unexpected GS-team row shape."
            ));
    }

    const Result<int> id = idFromValue(row.values[0]);
    if (!id)
    {
        return std::unexpected(id.error());
    }

    GsTeamMember member;
    member.id = *id;
    std::string* const fields[] = {
        &member.name,
        &member.koreanName,
        &member.position,
        &member.phoneNumber,
        &member.birthday
    };
    const std::string_view columnNames[] = {
        "name",
        "korean_name",
        "position",
        "phone_number",
        "birthday"
    };
    for (std::size_t index = 0; index < 5; ++index)
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

    return member;
}

SqliteParameters parameters(
    const GsTeamMember& member
    )
{
    return {
        SqliteValue{member.name},
        SqliteValue{member.koreanName},
        SqliteValue{member.position},
        SqliteValue{member.phoneNumber},
        SqliteValue{member.birthday}
    };
}
} // namespace

GsTeamService::GsTeamService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<std::vector<GsTeamMember>> GsTeamService::list() const
{
    const auto rows = m_database.query(
        "SELECT id, name, korean_name, position, phone_number, birthday "
        "FROM gs_team "
        "ORDER BY CASE position "
        "WHEN 'Branch Manager' THEN 1 "
        "WHEN 'M3' THEN 2 "
        "WHEN 'M2' THEN 3 "
        "WHEN 'M1' THEN 4 "
        "WHEN 'C3' THEN 5 "
        "WHEN 'C2' THEN 6 "
        "WHEN 'C1' THEN 7 "
        "ELSE 8 END, "
        "CASE WHEN name='' THEN korean_name ELSE name END COLLATE NOCASE, id"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<GsTeamMember> members;
    members.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<GsTeamMember> member = memberFromRow(row);
        if (!member)
        {
            return std::unexpected(member.error());
        }
        members.push_back(*member);
    }

    return members;
}

Status GsTeamService::saveDirectory(
    const std::vector<GsTeamMember>& members,
    const std::vector<int>& deletedIds
    )
{
    const ValidationResult validation = validateDirectory(members);
    if (validation.hasErrors())
    {
        const ValidationIssue& issue = validation.issues().front();
        return std::unexpected(error(
            ErrorCode::InvalidFormat,
            "GS-team directory validation failed: " + issue.code + "."
            ));
    }

    auto transaction = m_database.beginTransaction();
    if (!transaction)
    {
        return std::unexpected(transaction.error());
    }

    for (const int memberId : deletedIds)
    {
        if (memberId <= 0)
        {
            continue;
        }

        const Status deleted = m_database.execute(
            "DELETE FROM gs_team WHERE id=?",
            SqliteParameters{
                SqliteValue{std::int64_t{memberId}}
            }
            );
        if (!deleted)
        {
            return deleted;
        }
    }

    for (const GsTeamMember& source : members)
    {
        const GsTeamMember member = normalized(source);
        SqliteParameters values = parameters(member);
        if (member.id > 0)
        {
            values.push_back(SqliteValue{std::int64_t{member.id}});
            const Status updated = m_database.execute(
                "UPDATE gs_team SET name=?, korean_name=?, position=?, "
                "phone_number=?, birthday=? WHERE id=?",
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
                "INSERT INTO gs_team "
                "(name, korean_name, position, phone_number, birthday) "
                "VALUES (?, ?, ?, ?, ?)",
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

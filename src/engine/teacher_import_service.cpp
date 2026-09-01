#include "classmngr/engine/teacher_import_service.h"

#include "classmngr/engine/application_settings_service.h"
#include "classmngr/engine/sqlite_database.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace classmngr::engine
{
namespace
{
struct IsoDate
{
    int year = 0;
    int month = 0;
    int day = 0;
};

Error makeError(
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

bool isUtf8Continuation(
    unsigned char value
    ) noexcept
{
    return (value & 0xc0U) == 0x80U;
}

bool decodeUtf8(
    std::string_view value,
    std::size_t offset,
    std::uint32_t& codePoint,
    std::size_t& nextOffset
    ) noexcept
{
    if (offset >= value.size())
    {
        nextOffset = offset;
        return false;
    }

    const auto byteAt = [&value](std::size_t index) noexcept
    {
        return static_cast<unsigned char>(value[index]);
    };
    const unsigned char first = byteAt(offset);
    codePoint = first;
    nextOffset = offset + 1;

    if (first <= 0x7fU)
    {
        return true;
    }

    if (first >= 0xc2U && first <= 0xdfU)
    {
        if (offset + 1 >= value.size())
        {
            return false;
        }
        const unsigned char second = byteAt(offset + 1);
        if (!isUtf8Continuation(second))
        {
            return false;
        }

        codePoint = (static_cast<std::uint32_t>(first & 0x1fU) << 6)
            | static_cast<std::uint32_t>(second & 0x3fU);
        nextOffset = offset + 2;
        return true;
    }

    if (first >= 0xe0U && first <= 0xefU)
    {
        if (offset + 2 >= value.size())
        {
            return false;
        }
        const unsigned char second = byteAt(offset + 1);
        const unsigned char third = byteAt(offset + 2);
        if (!isUtf8Continuation(second) || !isUtf8Continuation(third))
        {
            return false;
        }
        if ((first == 0xe0U && second < 0xa0U)
            || (first == 0xedU && second >= 0xa0U))
        {
            return false;
        }

        codePoint = (static_cast<std::uint32_t>(first & 0x0fU) << 12)
            | (static_cast<std::uint32_t>(second & 0x3fU) << 6)
            | static_cast<std::uint32_t>(third & 0x3fU);
        nextOffset = offset + 3;
        return true;
    }

    if (first >= 0xf0U && first <= 0xf4U)
    {
        if (offset + 3 >= value.size())
        {
            return false;
        }
        const unsigned char second = byteAt(offset + 1);
        const unsigned char third = byteAt(offset + 2);
        const unsigned char fourth = byteAt(offset + 3);
        if (!isUtf8Continuation(second)
            || !isUtf8Continuation(third)
            || !isUtf8Continuation(fourth))
        {
            return false;
        }
        if ((first == 0xf0U && second < 0x90U)
            || (first == 0xf4U && second >= 0x90U))
        {
            return false;
        }

        codePoint = (static_cast<std::uint32_t>(first & 0x07U) << 18)
            | (static_cast<std::uint32_t>(second & 0x3fU) << 12)
            | (static_cast<std::uint32_t>(third & 0x3fU) << 6)
            | static_cast<std::uint32_t>(fourth & 0x3fU);
        nextOffset = offset + 4;
        return true;
    }

    return false;
}

void appendCodePoint(
    std::string& result,
    std::uint32_t codePoint
    )
{
    if (codePoint <= 0x7fU)
    {
        result.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint <= 0x7ffU)
    {
        result.push_back(static_cast<char>(0xc0U | (codePoint >> 6)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
    else if (codePoint <= 0xffffU
        && !(codePoint >= 0xd800U && codePoint <= 0xdfffU))
    {
        result.push_back(static_cast<char>(0xe0U | (codePoint >> 12)));
        result.push_back(static_cast<char>(
            0x80U | ((codePoint >> 6) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
    else if (codePoint <= 0x10ffffU)
    {
        result.push_back(static_cast<char>(0xf0U | (codePoint >> 18)));
        result.push_back(static_cast<char>(
            0x80U | ((codePoint >> 12) & 0x3fU)));
        result.push_back(static_cast<char>(
            0x80U | ((codePoint >> 6) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
}

bool isUnicodeWhitespace(
    std::uint32_t codePoint
    ) noexcept
{
    if (codePoint <= 0x7fU)
    {
        return std::isspace(static_cast<unsigned char>(codePoint)) != 0;
    }

    return codePoint == 0x85U
        || codePoint == 0xa0U
        || codePoint == 0x1680U
        || (codePoint >= 0x2000U && codePoint <= 0x200aU)
        || codePoint == 0x2028U
        || codePoint == 0x2029U
        || codePoint == 0x202fU
        || codePoint == 0x205fU
        || codePoint == 0x3000U;
}

std::string trimWhitespace(
    std::string_view value
    )
{
    std::size_t first = 0;
    while (first < value.size())
    {
        std::uint32_t codePoint = 0;
        std::size_t next = first;
        const bool valid = decodeUtf8(value, first, codePoint, next);
        if (!valid || !isUnicodeWhitespace(codePoint))
        {
            break;
        }
        first = next;
    }

    std::size_t last = value.size();
    std::size_t offset = 0;
    std::size_t lastNonWhitespace = 0;
    while (offset < value.size())
    {
        std::uint32_t codePoint = 0;
        std::size_t next = offset;
        const bool valid = decodeUtf8(value, offset, codePoint, next);
        if (!valid || !isUnicodeWhitespace(codePoint))
        {
            lastNonWhitespace = next;
        }
        offset = next;
    }
    last = lastNonWhitespace;

    if (first >= last)
    {
        return {};
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
    std::size_t offset = 0;
    while (offset < value.size())
    {
        std::uint32_t codePoint = 0;
        std::size_t next = offset;
        const bool valid = decodeUtf8(value, offset, codePoint, next);
        if (valid && isUnicodeWhitespace(codePoint))
        {
            pendingSpace = !result.empty();
            offset = next;
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }

        result.append(value.substr(offset, next - offset));
        offset = next;
    }

    return result;
}

void appendCaseFoldedCodePoint(
    std::string& result,
    std::uint32_t codePoint,
    std::string_view original
    )
{
    if (codePoint >= 'A' && codePoint <= 'Z')
    {
        appendCodePoint(result, codePoint + ('a' - 'A'));
        return;
    }

    // These mappings cover the case-folding forms used by ordinary Latin,
    // Greek, and Cyrillic names while keeping the engine independent of a
    // platform Unicode library.
    if ((codePoint >= 0xc0U && codePoint <= 0xd6U)
        || (codePoint >= 0xd8U && codePoint <= 0xdeU))
    {
        appendCodePoint(result, codePoint + 0x20U);
        return;
    }
    if (codePoint == 0xdfU || codePoint == 0x1e9eU)
    {
        result += "ss";
        return;
    }
    if (codePoint == 0x100U || codePoint == 0x102U
        || codePoint == 0x104U || codePoint == 0x106U
        || codePoint == 0x108U || codePoint == 0x10aU
        || codePoint == 0x10cU || codePoint == 0x10eU
        || codePoint == 0x110U || codePoint == 0x112U
        || codePoint == 0x114U || codePoint == 0x116U
        || codePoint == 0x118U || codePoint == 0x11aU
        || codePoint == 0x11cU || codePoint == 0x11eU
        || codePoint == 0x120U || codePoint == 0x122U
        || codePoint == 0x124U || codePoint == 0x126U
        || codePoint == 0x128U || codePoint == 0x12aU
        || codePoint == 0x12cU || codePoint == 0x12eU
        || codePoint == 0x130U || codePoint == 0x132U
        || codePoint == 0x134U || codePoint == 0x136U)
    {
        appendCodePoint(result, codePoint + 1U);
        return;
    }
    if (codePoint >= 0x139U && codePoint <= 0x14fU
        && (codePoint & 1U) != 0U)
    {
        appendCodePoint(result, codePoint + 1U);
        return;
    }
    if (codePoint >= 0x150U && codePoint <= 0x178U
        && (codePoint & 1U) == 0U
        && codePoint != 0x178U)
    {
        appendCodePoint(result, codePoint + 1U);
        return;
    }
    if (codePoint == 0x178U)
    {
        appendCodePoint(result, 0xffU);
        return;
    }
    if (codePoint >= 0x179U && codePoint <= 0x17dU
        && (codePoint & 1U) != 0U)
    {
        appendCodePoint(result, codePoint + 1U);
        return;
    }
    if (codePoint == 0x17fU)
    {
        result.push_back('s');
        return;
    }
    if (codePoint >= 0x391U && codePoint <= 0x3abu
        && codePoint != 0x3a2U)
    {
        appendCodePoint(result, codePoint + 0x20U);
        return;
    }
    if (codePoint == 0x3c2U)
    {
        appendCodePoint(result, 0x3c3U);
        return;
    }
    if (codePoint >= 0x400U && codePoint <= 0x40fU)
    {
        appendCodePoint(result, codePoint + 0x50U);
        return;
    }
    if (codePoint >= 0x410U && codePoint <= 0x42fU)
    {
        appendCodePoint(result, codePoint + 0x20U);
        return;
    }
    if (codePoint == 0x2126U)
    {
        appendCodePoint(result, 0x3c9U);
        return;
    }
    if (codePoint == 0x212aU)
    {
        result.push_back('k');
        return;
    }
    if (codePoint == 0x212bU)
    {
        appendCodePoint(result, 0xe5U);
        return;
    }
    switch (codePoint)
    {
    case 0xfb00U:
        result += "ff";
        return;
    case 0xfb01U:
        result += "fi";
        return;
    case 0xfb02U:
        result += "fl";
        return;
    case 0xfb03U:
        result += "ffi";
        return;
    case 0xfb04U:
        result += "ffl";
        return;
    case 0xfb05U:
    case 0xfb06U:
        result += "st";
        return;
    default:
        break;
    }

    result.append(original);
}

std::string caseFolded(
    std::string_view value
    )
{
    std::string result;
    result.reserve(value.size());
    std::size_t offset = 0;
    while (offset < value.size())
    {
        std::uint32_t codePoint = 0;
        std::size_t next = offset;
        const bool valid = decodeUtf8(value, offset, codePoint, next);
        if (!valid)
        {
            result.append(value.substr(offset, next - offset));
        }
        else
        {
            appendCaseFoldedCodePoint(
                result,
                codePoint,
                value.substr(offset, next - offset)
                );
        }
        offset = next;
    }
    return result;
}

std::string normalizedName(
    std::string_view value
    )
{
    return caseFolded(simplified(value));
}

bool isHangul(
    std::uint32_t codePoint
    ) noexcept
{
    return (codePoint >= 0x1100U && codePoint <= 0x11ffU)
        || (codePoint >= 0x3130U && codePoint <= 0x318fU)
        || (codePoint >= 0xa960U && codePoint <= 0xa97fU)
        || (codePoint >= 0xac00U && codePoint <= 0xd7afU)
        || (codePoint >= 0xd7b0U && codePoint <= 0xd7ffU);
}

std::string hangulOnly(
    std::string_view value
    )
{
    std::string result;
    result.reserve(value.size());
    std::size_t offset = 0;
    while (offset < value.size())
    {
        std::uint32_t codePoint = 0;
        std::size_t next = offset;
        const bool valid = decodeUtf8(value, offset, codePoint, next);
        if (valid && isHangul(codePoint))
        {
            result.append(value.substr(offset, next - offset));
        }
        offset = next;
    }
    return result;
}

bool isLeapYear(
    int year
    ) noexcept
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(
    int year,
    int month
    ) noexcept
{
    switch (month)
    {
    case 2:
        return isLeapYear(year) ? 29 : 28;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    default:
        return 31;
    }
}

bool parseIsoDate(
    std::string_view value,
    IsoDate& date
    ) noexcept
{
    if (value.size() != 10 || value[4] != '-' || value[7] != '-')
    {
        return false;
    }

    const auto digits = [&value](std::size_t first, std::size_t count)
    {
        int result = 0;
        for (std::size_t index = first; index < first + count; ++index)
        {
            const unsigned char character =
                static_cast<unsigned char>(value[index]);
            if (character < '0' || character > '9')
            {
                return -1;
            }
            result = result * 10 + static_cast<int>(character - '0');
        }
        return result;
    };

    date.year = digits(0, 4);
    date.month = digits(5, 2);
    date.day = digits(8, 2);
    return date.year >= 1 && date.year <= 9999
        && date.month >= 1 && date.month <= 12
        && date.day >= 1 && date.day <= daysInMonth(date.year, date.month);
}

bool dateLessOrEqual(
    const IsoDate& lhs,
    const IsoDate& rhs
    ) noexcept
{
    if (lhs.year != rhs.year)
    {
        return lhs.year < rhs.year;
    }
    if (lhs.month != rhs.month)
    {
        return lhs.month < rhs.month;
    }
    return lhs.day <= rhs.day;
}

Status validatePlan(
    const TeacherImportPlan& plan
    )
{
    IsoDate sourceDate;
    if (!parseIsoDate(plan.sourceDate, sourceDate))
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidFormat,
            "The teacher import source date must be a valid ISO date."
            ));
    }

    std::unordered_set<std::string> koreanNames;
    koreanNames.reserve(plan.koreanTeachers.size());
    for (const Teacher& teacher : plan.koreanTeachers)
    {
        const std::string key = hangulOnly(teacher.teacherKr);
        if (key.empty())
        {
            return std::unexpected(makeError(
                ErrorCode::InvalidFormat,
                "Every imported Korean teacher must have a name."
                ));
        }
        if (!koreanNames.insert(key).second)
        {
            return std::unexpected(makeError(
                ErrorCode::InvalidFormat,
                "The import contains a duplicate Korean teacher name."
                ));
        }
    }

    std::unordered_set<std::string> nativeNames;
    nativeNames.reserve(plan.nativeEnglishTeachers.size());
    for (const NativeEnglishTeacher& teacher : plan.nativeEnglishTeachers)
    {
        const std::string key = normalizedName(teacher.name);
        if (key.empty())
        {
            return std::unexpected(makeError(
                ErrorCode::InvalidFormat,
                "Every imported Native English Teacher must have a name."
                ));
        }
        if (!nativeNames.insert(key).second)
        {
            return std::unexpected(makeError(
                ErrorCode::InvalidFormat,
                "The import contains a duplicate Native English Teacher name."
                ));
        }
    }

    std::unordered_set<std::string> gsEnglishNames;
    std::unordered_set<std::string> gsKoreanNames;
    gsEnglishNames.reserve(plan.gsTeamMembers.size());
    gsKoreanNames.reserve(plan.gsTeamMembers.size());
    for (const GsTeamMember& member : plan.gsTeamMembers)
    {
        const std::string english = normalizedName(member.name);
        const std::string korean = normalizedName(member.koreanName);
        if (english.empty() && korean.empty())
        {
            return std::unexpected(makeError(
                ErrorCode::InvalidFormat,
                "Every imported GS Team member must have a name."
                ));
        }
        if ((!english.empty() && !gsEnglishNames.insert(english).second)
            || (!korean.empty() && !gsKoreanNames.insert(korean).second))
        {
            return std::unexpected(makeError(
                ErrorCode::InvalidFormat,
                "The import contains a duplicate GS Team name."
                ));
        }
    }

    return {};
}

Result<int> idFromValue(
    const SqliteValue& value,
    std::string_view kind
    )
{
    const auto* id = std::get_if<std::int64_t>(&value);
    if (id == nullptr
        || *id <= 0
        || *id > std::numeric_limits<int>::max())
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "SQLite returned an invalid " + std::string(kind) + " id."
            ));
    }

    return static_cast<int>(*id);
}

Result<std::string> textFromValue(
    const SqliteValue& value,
    std::string_view kind,
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

    return std::unexpected(makeError(
        ErrorCode::Schema,
        "SQLite returned a non-text " + std::string(kind)
            + " " + std::string(column) + " value."
        ));
}

Result<Teacher> teacherFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 15)
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "SQLite returned an unexpected Korean teacher row shape."
            ));
    }

    const Result<int> id = idFromValue(row.values[0], "teacher");
    if (!id)
    {
        return std::unexpected(id.error());
    }

    Teacher teacher;
    teacher.id = *id;
    std::string* const fields[] = {
        &teacher.teacherKr,
        &teacher.teacherEn,
        &teacher.preferredRomanization,
        &teacher.preferredName,
        &teacher.roomNumber,
        &teacher.birthday,
        &teacher.phoneNumber,
        &teacher.wifiName,
        &teacher.wifiPassword,
        &teacher.internetType,
        &teacher.zoomId,
        &teacher.zoomPassword,
        &teacher.projectionType,
        &teacher.notes
    };
    const std::string_view columns[] = {
        "teacher_kr",
        "teacher_en",
        "preferred_romanization",
        "preferred_name",
        "room_number",
        "birthday",
        "phone_number",
        "wifi_name",
        "wifi_password",
        "internet_type",
        "zoom_id",
        "zoom_password",
        "projection_type",
        "notes"
    };
    for (std::size_t index = 0; index < 14; ++index)
    {
        const Result<std::string> value = textFromValue(
            row.values[index + 1],
            "teacher",
            columns[index]
            );
        if (!value)
        {
            return std::unexpected(value.error());
        }
        *fields[index] = *value;
    }

    return teacher;
}

Result<NativeEnglishTeacher> nativeTeacherFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 7)
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "SQLite returned an unexpected Native English Teacher row shape."
            ));
    }

    const Result<int> id = idFromValue(
        row.values[0],
        "Native English Teacher"
        );
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
    const std::string_view columns[] = {
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
            "Native English Teacher",
            columns[index]
            );
        if (!value)
        {
            return std::unexpected(value.error());
        }
        *fields[index] = *value;
    }

    return teacher;
}

Result<GsTeamMember> gsMemberFromRow(
    const SqliteRow& row
    )
{
    if (row.values.size() != 6)
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "SQLite returned an unexpected GS Team row shape."
            ));
    }

    const Result<int> id = idFromValue(row.values[0], "GS Team member");
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
    const std::string_view columns[] = {
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
            "GS Team member",
            columns[index]
            );
        if (!value)
        {
            return std::unexpected(value.error());
        }
        *fields[index] = *value;
    }

    return member;
}

Result<std::vector<Teacher>> loadKoreanTeachers(
    SqliteDatabase& database
    )
{
    const auto rows = database.query(
        "SELECT id, teacher_kr, teacher_en, preferred_romanization, "
        "preferred_name, room_number, birthday, phone_number, wifi_name, "
        "wifi_password, internet_type, zoom_id, zoom_password, "
        "projection_type, notes FROM teachers"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<Teacher> teachers;
    teachers.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<Teacher> teacher = teacherFromRow(row);
        if (!teacher)
        {
            return std::unexpected(teacher.error());
        }
        teachers.push_back(*teacher);
    }
    return teachers;
}

Result<std::vector<NativeEnglishTeacher>> loadNativeTeachers(
    SqliteDatabase& database
    )
{
    const auto rows = database.query(
        "SELECT id, name, position, phone_number, birthday, nationality, "
        "email FROM native_english_teachers"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<NativeEnglishTeacher> teachers;
    teachers.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<NativeEnglishTeacher> teacher = nativeTeacherFromRow(row);
        if (!teacher)
        {
            return std::unexpected(teacher.error());
        }
        teachers.push_back(*teacher);
    }
    return teachers;
}

Result<std::vector<GsTeamMember>> loadGsTeam(
    SqliteDatabase& database
    )
{
    const auto rows = database.query(
        "SELECT id, name, korean_name, position, phone_number, birthday "
        "FROM gs_team"
        );
    if (!rows)
    {
        return std::unexpected(rows.error());
    }

    std::vector<GsTeamMember> members;
    members.reserve(rows->rows.size());
    for (const SqliteRow& row : rows->rows)
    {
        const Result<GsTeamMember> member = gsMemberFromRow(row);
        if (!member)
        {
            return std::unexpected(member.error());
        }
        members.push_back(*member);
    }
    return members;
}

template<typename T, typename Name>
std::vector<std::size_t> matchingIndexes(
    const std::vector<T>& values,
    std::string_view key,
    Name name
    )
{
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (normalizedName(name(values[index])) == key)
        {
            result.push_back(index);
        }
    }
    return result;
}

std::vector<std::size_t> matchingKoreanTeacherIndexes(
    const std::vector<Teacher>& teachers,
    std::string_view key
    )
{
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < teachers.size(); ++index)
    {
        if (hangulOnly(teachers[index].teacherKr) == key)
        {
            result.push_back(index);
        }
    }
    return result;
}

Status updateLatestDate(
    SqliteDatabase& database,
    const IsoDate& sourceDate,
    std::string_view sourceDateText
    )
{
    ApplicationSettingsService settings(database);
    const Result<SettingValue> current = settings.load(
        TeacherImportService::LatestSourceDateSetting
        );
    if (!current)
    {
        return std::unexpected(current.error());
    }

    std::string currentText;
    if (const auto* text = std::get_if<std::string>(&*current))
    {
        currentText = *text;
    }
    else if (!std::holds_alternative<std::monostate>(*current))
    {
        return std::unexpected(makeError(
            ErrorCode::Schema,
            "SQLite returned a non-text teacher import setting value."
            ));
    }

    IsoDate currentDate;
    if (parseIsoDate(currentText, currentDate)
        && dateLessOrEqual(sourceDate, currentDate))
    {
        return {};
    }

    return settings.save(
        TeacherImportService::LatestSourceDateSetting,
        SettingValue{std::string(sourceDateText)}
        );
}
} // namespace

TeacherImportService::TeacherImportService(
    SqliteDatabase& database
    )
    : m_database(database)
{
}

Result<TeacherImportSummary> TeacherImportService::importTeachers(
    const TeacherImportPlan& plan
    )
{
    const Status valid = validatePlan(plan);
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    IsoDate sourceDate;
    if (!parseIsoDate(plan.sourceDate, sourceDate))
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidFormat,
            "The teacher import source date must be a valid ISO date."
            ));
    }

    auto transaction = m_database.beginTransaction();
    if (!transaction)
    {
        return std::unexpected(transaction.error());
    }

    const Result<std::vector<Teacher>> koreanResult =
        loadKoreanTeachers(m_database);
    const Result<std::vector<NativeEnglishTeacher>> nativeResult =
        loadNativeTeachers(m_database);
    const Result<std::vector<GsTeamMember>> gsResult = loadGsTeam(m_database);
    if (!koreanResult)
    {
        return std::unexpected(koreanResult.error());
    }
    if (!nativeResult)
    {
        return std::unexpected(nativeResult.error());
    }
    if (!gsResult)
    {
        return std::unexpected(gsResult.error());
    }

    std::vector<Teacher> korean = *koreanResult;
    std::vector<NativeEnglishTeacher> native = *nativeResult;
    std::vector<GsTeamMember> gs = *gsResult;
    TeacherImportSummary summary;

    for (const Teacher& source : plan.koreanTeachers)
    {
        const std::string key = hangulOnly(source.teacherKr);
        const std::vector<std::size_t> matches =
            matchingKoreanTeacherIndexes(korean, key);
        if (matches.size() > 1)
        {
            return std::unexpected(makeError(
                ErrorCode::Constraint,
                "More than one stored Korean teacher matches "
                    + source.teacherKr + "."
                ));
        }

        const std::string teacherEn = trimWhitespace(source.teacherEn);
        const std::string preferredRomanization =
            trimWhitespace(source.preferredRomanization);
        const std::string preferredName = trimWhitespace(source.preferredName);
        const std::string roomNumber = trimWhitespace(source.roomNumber);
        const std::string birthday = trimWhitespace(source.birthday);
        const std::string phoneNumber = trimWhitespace(source.phoneNumber);
        const std::string wifiName = trimWhitespace(source.wifiName);
        const std::string wifiPassword = trimWhitespace(source.wifiPassword);
        const std::string internetType = trimWhitespace(source.internetType);
        const std::string zoomId = trimWhitespace(source.zoomId);
        const std::string zoomPassword = trimWhitespace(source.zoomPassword);
        const std::string projectionType = trimWhitespace(source.projectionType);
        const std::string notes = trimWhitespace(source.notes);

        if (matches.empty())
        {
            const Status inserted = m_database.execute(
                "INSERT INTO teachers "
                "(teacher_kr, teacher_en, preferred_romanization, "
                "preferred_name, room_number, birthday, phone_number, "
                "wifi_name, wifi_password, internet_type, zoom_id, "
                "zoom_password, projection_type, notes) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{key},
                    SqliteValue{teacherEn},
                    SqliteValue{preferredRomanization},
                    SqliteValue{preferredName},
                    SqliteValue{roomNumber},
                    SqliteValue{birthday},
                    SqliteValue{phoneNumber},
                    SqliteValue{wifiName},
                    SqliteValue{wifiPassword},
                    SqliteValue{internetType},
                    SqliteValue{zoomId},
                    SqliteValue{zoomPassword},
                    SqliteValue{projectionType},
                    SqliteValue{notes}
                }
                );
            if (!inserted)
            {
                return std::unexpected(inserted.error());
            }
            ++summary.koreanTeachers.created;
            continue;
        }

        const Teacher& existing = korean[matches.front()];
        const std::string room = roomNumber.empty()
            ? existing.roomNumber
            : roomNumber;
        const std::string existingBirthday = birthday.empty()
            ? existing.birthday
            : birthday;
        const std::string phone = phoneNumber.empty()
            ? existing.phoneNumber
            : phoneNumber;
        if (key == existing.teacherKr
            && room == existing.roomNumber
            && existingBirthday == existing.birthday
            && phone == existing.phoneNumber)
        {
            ++summary.koreanTeachers.unchanged;
            continue;
        }

        const Status updated = m_database.execute(
            "UPDATE teachers SET teacher_kr=?, room_number=?, birthday=?, "
            "phone_number=? WHERE id=?",
            SqliteParameters{
                SqliteValue{key},
                SqliteValue{room},
                SqliteValue{existingBirthday},
                SqliteValue{phone},
                SqliteValue{std::int64_t{existing.id}}
            }
            );
        if (!updated)
        {
            return std::unexpected(updated.error());
        }
        ++summary.koreanTeachers.updated;
    }

    for (const NativeEnglishTeacher& source : plan.nativeEnglishTeachers)
    {
        const std::string key = normalizedName(source.name);
        const std::vector<std::size_t> matches = matchingIndexes(
            native,
            key,
            [](const NativeEnglishTeacher& teacher) -> const std::string&
            {
                return teacher.name;
            }
            );
        if (matches.size() > 1)
        {
            return std::unexpected(makeError(
                ErrorCode::Constraint,
                "More than one stored Native English Teacher matches "
                    + source.name + "."
                ));
        }

        const std::string name = simplified(source.name);
        const std::string position = trimWhitespace(source.position);
        const std::string phoneNumber = trimWhitespace(source.phoneNumber);
        const std::string birthday = trimWhitespace(source.birthday);
        const std::string nationality = trimWhitespace(source.nationality);
        const std::string email = trimWhitespace(source.email);

        if (matches.empty())
        {
            const Status inserted = m_database.execute(
                "INSERT INTO native_english_teachers "
                "(name, position, phone_number, birthday, nationality, email) "
                "VALUES (?, ?, ?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{name},
                    SqliteValue{position},
                    SqliteValue{phoneNumber},
                    SqliteValue{birthday},
                    SqliteValue{nationality},
                    SqliteValue{email}
                }
                );
            if (!inserted)
            {
                return std::unexpected(inserted.error());
            }
            ++summary.nativeEnglishTeachers.created;
            continue;
        }

        const NativeEnglishTeacher& existing = native[matches.front()];
        NativeEnglishTeacher updatedTeacher = existing;
        updatedTeacher.name = name;
        if (!position.empty())
        {
            updatedTeacher.position = position;
        }
        if (!phoneNumber.empty())
        {
            updatedTeacher.phoneNumber = phoneNumber;
        }
        if (!birthday.empty())
        {
            updatedTeacher.birthday = birthday;
        }
        if (!nationality.empty())
        {
            updatedTeacher.nationality = nationality;
        }
        if (!email.empty())
        {
            updatedTeacher.email = email;
        }
        if (updatedTeacher.name == existing.name
            && updatedTeacher.position == existing.position
            && updatedTeacher.phoneNumber == existing.phoneNumber
            && updatedTeacher.birthday == existing.birthday
            && updatedTeacher.nationality == existing.nationality
            && updatedTeacher.email == existing.email)
        {
            ++summary.nativeEnglishTeachers.unchanged;
            continue;
        }

        const Status updated = m_database.execute(
            "UPDATE native_english_teachers SET name=?, position=?, "
            "phone_number=?, birthday=?, nationality=?, email=? WHERE id=?",
            SqliteParameters{
                SqliteValue{updatedTeacher.name},
                SqliteValue{updatedTeacher.position},
                SqliteValue{updatedTeacher.phoneNumber},
                SqliteValue{updatedTeacher.birthday},
                SqliteValue{updatedTeacher.nationality},
                SqliteValue{updatedTeacher.email},
                SqliteValue{std::int64_t{existing.id}}
            }
            );
        if (!updated)
        {
            return std::unexpected(updated.error());
        }
        ++summary.nativeEnglishTeachers.updated;
    }

    for (const GsTeamMember& source : plan.gsTeamMembers)
    {
        const std::string sourceName = trimWhitespace(source.name);
        const std::string sourceKoreanName =
            trimWhitespace(source.koreanName);
        const bool useKorean = !sourceKoreanName.empty();
        const std::string key = normalizedName(
            useKorean ? source.koreanName : source.name
            );
        const std::vector<std::size_t> matches = useKorean
            ? matchingIndexes(
                gs,
                key,
                [](const GsTeamMember& member) -> const std::string&
                {
                    return member.koreanName;
                }
                )
            : matchingIndexes(
                gs,
                key,
                [](const GsTeamMember& member) -> const std::string&
                {
                    return member.name;
                }
                );
        if (matches.size() > 1)
        {
            return std::unexpected(makeError(
                ErrorCode::Constraint,
                "More than one stored GS Team member matches "
                    + (useKorean ? source.koreanName : source.name) + "."
                ));
        }

        const std::string name = simplified(source.name);
        const std::string koreanName = simplified(source.koreanName);
        const std::string position = trimWhitespace(source.position);
        const std::string phoneNumber = trimWhitespace(source.phoneNumber);
        const std::string birthday = trimWhitespace(source.birthday);

        if (matches.empty())
        {
            const Status inserted = m_database.execute(
                "INSERT INTO gs_team "
                "(name, korean_name, position, phone_number, birthday) "
                "VALUES (?, ?, ?, ?, ?)",
                SqliteParameters{
                    SqliteValue{name},
                    SqliteValue{koreanName},
                    SqliteValue{position},
                    SqliteValue{phoneNumber},
                    SqliteValue{birthday}
                }
                );
            if (!inserted)
            {
                return std::unexpected(inserted.error());
            }
            ++summary.gsTeamMembers.created;
            continue;
        }

        const GsTeamMember& existing = gs[matches.front()];
        GsTeamMember updatedMember = existing;
        if (!sourceName.empty())
        {
            updatedMember.name = name;
        }
        if (!sourceKoreanName.empty())
        {
            updatedMember.koreanName = koreanName;
        }
        if (!position.empty())
        {
            updatedMember.position = position;
        }
        if (!phoneNumber.empty())
        {
            updatedMember.phoneNumber = phoneNumber;
        }
        if (!birthday.empty())
        {
            updatedMember.birthday = birthday;
        }
        if (updatedMember.name == existing.name
            && updatedMember.koreanName == existing.koreanName
            && updatedMember.position == existing.position
            && updatedMember.phoneNumber == existing.phoneNumber
            && updatedMember.birthday == existing.birthday)
        {
            ++summary.gsTeamMembers.unchanged;
            continue;
        }

        const Status updated = m_database.execute(
            "UPDATE gs_team SET name=?, korean_name=?, position=?, "
            "phone_number=?, birthday=? WHERE id=?",
            SqliteParameters{
                SqliteValue{updatedMember.name},
                SqliteValue{updatedMember.koreanName},
                SqliteValue{updatedMember.position},
                SqliteValue{updatedMember.phoneNumber},
                SqliteValue{updatedMember.birthday},
                SqliteValue{std::int64_t{existing.id}}
            }
            );
        if (!updated)
        {
            return std::unexpected(updated.error());
        }
        ++summary.gsTeamMembers.updated;
    }

    const Status dateStatus = updateLatestDate(
        m_database,
        sourceDate,
        plan.sourceDate
        );
    if (!dateStatus)
    {
        return std::unexpected(dateStatus.error());
    }

    const Status committed = transaction->commit();
    if (!committed)
    {
        return std::unexpected(committed.error());
    }

    return summary;
}

} // namespace classmngr::engine

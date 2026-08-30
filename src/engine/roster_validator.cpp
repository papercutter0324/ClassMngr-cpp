#include "classmngr/engine/roster_validator.h"

#include "classmngr/engine/student_name.h"

#include <cctype>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace classmngr::engine
{
namespace
{
struct DecodedText
{
    std::vector<unsigned> codePoints;
    bool valid = true;
};

bool decodeUtf8(
    std::string_view value,
    std::size_t* offset,
    unsigned* codePoint
    )
{
    if (offset == nullptr || codePoint == nullptr || *offset >= value.size())
    {
        return false;
    }

    const auto byteAt = [&value](std::size_t index)
    {
        return static_cast<unsigned char>(value[index]);
    };

    const unsigned first = byteAt(*offset);
    if (first <= 0x7fU)
    {
        ++*offset;
        *codePoint = first;
        return true;
    }

    unsigned length = 0;
    unsigned result = 0;
    unsigned minimum = 0;
    if (first >= 0xc2U && first <= 0xdfU)
    {
        length = 2;
        result = first & 0x1fU;
        minimum = 0x80U;
    }
    else if (first >= 0xe0U && first <= 0xefU)
    {
        length = 3;
        result = first & 0x0fU;
        minimum = 0x800U;
    }
    else if (first >= 0xf0U && first <= 0xf4U)
    {
        length = 4;
        result = first & 0x07U;
        minimum = 0x10000U;
    }
    else
    {
        return false;
    }

    if (*offset + length > value.size())
    {
        return false;
    }

    for (unsigned index = 1; index < length; ++index)
    {
        const unsigned continuation = byteAt(*offset + index);
        if ((continuation & 0xc0U) != 0x80U)
        {
            return false;
        }
        result = (result << 6U) | (continuation & 0x3fU);
    }

    if (result < minimum || result > 0x10ffffU
        || (result >= 0xd800U && result <= 0xdfffU))
    {
        return false;
    }

    *offset += length;
    *codePoint = result;
    return true;
}

DecodedText decode(std::string_view value)
{
    DecodedText result;
    std::size_t offset = 0;
    while (offset < value.size())
    {
        unsigned codePoint = 0;
        if (!decodeUtf8(value, &offset, &codePoint))
        {
            result.valid = false;
            return result;
        }
        result.codePoints.push_back(codePoint);
    }
    return result;
}

bool isWhitespace(unsigned codePoint)
{
    if (codePoint <= 0x7fU)
    {
        return std::isspace(static_cast<unsigned char>(codePoint)) != 0;
    }

    return (codePoint >= 0x2000U && codePoint <= 0x200aU)
        || codePoint == 0x2028U
        || codePoint == 0x2029U
        || codePoint == 0x202fU
        || codePoint == 0x205fU
        || codePoint == 0x3000U;
}

std::string trimAsciiWhitespace(std::string_view value)
{
    const auto isAsciiWhitespace = [](char character)
    {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    };

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

std::string simplifyAsciiWhitespace(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;

    for (const unsigned char character : value)
    {
        if (std::isspace(character) != 0)
        {
            pendingSpace = !result.empty();
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(static_cast<char>(character));
    }

    return result;
}

bool isAsciiLetter(unsigned codePoint)
{
    return (codePoint >= 'A' && codePoint <= 'Z')
        || (codePoint >= 'a' && codePoint <= 'z');
}

bool equalsAsciiInsensitive(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const unsigned char leftCharacter =
            static_cast<unsigned char>(left[index]);
        const unsigned char rightCharacter =
            static_cast<unsigned char>(right[index]);
        if (std::tolower(leftCharacter) != std::tolower(rightCharacter))
        {
            return false;
        }
    }

    return true;
}

std::string canonicalColumnName(std::string_view value)
{
    const std::string simplified = simplifyAsciiWhitespace(value);
    for (const std::string_view requiredColumn : RosterBaseColumns)
    {
        if (equalsAsciiInsensitive(simplified, requiredColumn))
        {
            return std::string(requiredColumn);
        }
    }

    if (equalsAsciiInsensitive(simplified, "Autumn"))
    {
        return "Fall";
    }

    return simplified;
}

int columnIndex(
    const std::vector<std::string>& columns,
    std::string_view name
    )
{
    for (std::size_t index = 0; index < columns.size(); ++index)
    {
        if (equalsAsciiInsensitive(columns[index], name))
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

bool rowHasData(const std::vector<std::string>& row)
{
    for (const std::string& value : row)
    {
        if (!trimAsciiWhitespace(value).empty())
        {
            return true;
        }
    }

    return false;
}

std::size_t utf8Length(std::string_view value)
{
    const DecodedText decoded = decode(value);
    if (!decoded.valid)
    {
        return value.size();
    }

    return decoded.codePoints.size();
}

ValidationIssue issue(
    std::string code,
    std::string field = {},
    ValidationSeverity severity = ValidationSeverity::Error,
    int row = -1,
    int column = -1
    )
{
    return {
        .code = std::move(code),
        .field = std::move(field),
        .severity = severity,
        .row = row,
        .column = column
    };
}

void addTextLengthIssue(
    ValidationResult& result,
    std::string_view value,
    std::size_t minimum,
    std::size_t maximum,
    std::string field,
    int row = -1,
    int column = -1
    )
{
    const std::size_t length = utf8Length(value);
    if (length < minimum || length > maximum)
    {
        result.add(issue(
            "validation.length.out_of_bounds",
            std::move(field),
            ValidationSeverity::Error,
            row,
            column
            ));
    }
}

void addEnglishNameIssues(
    ValidationResult& result,
    const std::string& value,
    int row,
    int column,
    std::string field
    )
{
    if (utf8Length(value) > 20)
    {
        result.add(issue(
            "student_name.english.too_long",
            field,
            ValidationSeverity::Error,
            row,
            column
            ));
    }

    bool nonAscii = false;
    bool invalid = false;
    const DecodedText decoded = decode(value);
    if (!decoded.valid)
    {
        nonAscii = true;
        invalid = true;
    }
    else
    {
        for (const unsigned codePoint : decoded.codePoints)
        {
            if (codePoint > 127U)
            {
                nonAscii = true;
                continue;
            }

            if (isAsciiLetter(codePoint)
                || codePoint == '.'
                || codePoint == '-'
                || isWhitespace(codePoint))
            {
                continue;
            }

            invalid = true;
        }
    }

    if (nonAscii)
    {
        result.add(issue(
            "student_name.english.non_ascii",
            field,
            ValidationSeverity::Error,
            row,
            column
            ));
    }

    if (invalid)
    {
        result.add(issue(
            "student_name.english.invalid_characters",
            std::move(field),
            ValidationSeverity::Error,
            row,
            column
            ));
    }
}

bool validKoreanShape(
    const std::string& value
    )
{
    const DecodedText decoded = decode(value);
    if (!decoded.valid || decoded.codePoints.empty())
    {
        return false;
    }

    std::size_t begin = 0;
    std::size_t end = decoded.codePoints.size();
    while (begin < end && isWhitespace(decoded.codePoints[begin]))
    {
        ++begin;
    }
    while (end > begin && isWhitespace(decoded.codePoints[end - 1]))
    {
        --end;
    }

    if (begin == end)
    {
        return false;
    }

    if (end - begin >= 3
        && decoded.codePoints[end - 3] == '('
        && isAsciiLetter(decoded.codePoints[end - 2])
        && decoded.codePoints[end - 1] == ')')
    {
        end -= 3;
        while (end > begin && isWhitespace(decoded.codePoints[end - 1]))
        {
            --end;
        }
    }

    bool hasHangul = false;
    for (std::size_t index = begin; index < end; ++index)
    {
        const unsigned codePoint = decoded.codePoints[index];
        if (codePoint >= 0xac00U && codePoint <= 0xd7a3U)
        {
            hasHangul = true;
            continue;
        }

        if (!isWhitespace(codePoint))
        {
            return false;
        }
    }

    return hasHangul;
}

void addKoreanNameIssues(
    ValidationResult& result,
    const std::string& value,
    int row,
    int column,
    std::string field,
    bool allowQuestionableLength
    )
{
    if (!validKoreanShape(value))
    {
        result.add(issue(
            "student_name.korean.invalid_characters",
            field,
            ValidationSeverity::Error,
            row,
            column
            ));
    }

    const std::size_t length = utf8Length(
        StudentNameService::baseKorean(value));
    const ValidationSeverity questionableSeverity = allowQuestionableLength
        ? ValidationSeverity::Warning
        : ValidationSeverity::Error;

    if (length == 0 || length == 3)
    {
        return;
    }

    if (length <= 1)
    {
        result.add(issue(
            "student_name.korean.too_short",
            field,
            questionableSeverity,
            row,
            column
            ));
        return;
    }

    if (length >= 5)
    {
        result.add(issue(
            "student_name.korean.too_long",
            field,
            questionableSeverity,
            row,
            column
            ));
        return;
    }

    result.add(issue(
        "student_name.korean.unusual_length",
        std::move(field),
        ValidationSeverity::Warning,
        row,
        column
        ));
}

void addRequiredNameIssue(
    ValidationResult& result,
    const std::vector<std::string>& rowValues,
    int row,
    int column,
    std::string field
    )
{
    if (column >= 0
        && static_cast<std::size_t>(column) < rowValues.size()
        && !trimAsciiWhitespace(rowValues[static_cast<std::size_t>(column)]).empty())
    {
        return;
    }

    result.add(issue(
        "roster.student_name.required",
        std::move(field),
        ValidationSeverity::Error,
        row,
        column
        ));
}

std::string namePairKey(
    const std::vector<std::string>& row,
    int englishColumn,
    int koreanColumn
    )
{
    if (englishColumn < 0 || koreanColumn < 0
        || static_cast<std::size_t>(englishColumn) >= row.size()
        || static_cast<std::size_t>(koreanColumn) >= row.size())
    {
        return {};
    }

    const std::string english = trimAsciiWhitespace(
        row[static_cast<std::size_t>(englishColumn)]);
    const std::string korean = trimAsciiWhitespace(
        row[static_cast<std::size_t>(koreanColumn)]);
    if (english.empty() || korean.empty())
    {
        return {};
    }

    std::string key;
    key.reserve(english.size() + korean.size() + 1);
    key += english;
    key.push_back('\x1f');
    key += korean;
    return key;
}
} // namespace

Roster RosterValidator::normalized(const Roster& roster)
{
    Roster normalized = roster;
    normalized.columns.clear();
    normalized.columns.reserve(roster.columns.size());
    for (const std::string& column : roster.columns)
    {
        normalized.columns.push_back(canonicalColumnName(column));
    }

    const int englishColumn = columnIndex(
        normalized.columns, "English");
    const int koreanColumn = columnIndex(
        normalized.columns, "Korean");

    normalized.rows.clear();
    normalized.rows.reserve(roster.rows.size());
    for (const std::vector<std::string>& sourceRow : roster.rows)
    {
        std::vector<std::string> row;
        row.reserve(sourceRow.size());
        for (std::size_t column = 0; column < sourceRow.size(); ++column)
        {
            const std::string& value = sourceRow[column];
            if (static_cast<int>(column) == englishColumn)
            {
                row.push_back(StudentNameService::normalizeEnglish(value));
            }
            else if (static_cast<int>(column) == koreanColumn)
            {
                row.push_back(StudentNameService::normalizeKorean(value));
            }
            else
            {
                row.push_back(simplifyAsciiWhitespace(value));
            }
        }
        normalized.rows.push_back(std::move(row));
    }

    return normalized;
}

ValidationResult RosterValidator::validate(
    const Roster& roster,
    bool allowQuestionableKoreanNameLengths
    )
{
    ValidationResult result;
    const int englishColumn = columnIndex(roster.columns, "English");
    const int koreanColumn = columnIndex(roster.columns, "Korean");

    for (const std::string_view requiredColumn : RosterBaseColumns)
    {
        if (columnIndex(roster.columns, requiredColumn) < 0)
        {
            result.add(issue(
                "roster.column.required",
                "columns",
                ValidationSeverity::Error
                ));
        }
    }

    for (std::size_t column = 0; column < roster.columns.size(); ++column)
    {
        const std::string& name = roster.columns[column];
        addTextLengthIssue(
            result,
            name,
            1,
            RosterValidator::MaximumColumnNameLength,
            "columns[" + std::to_string(column) + "]",
            -1,
            static_cast<int>(column)
            );

        for (std::size_t previous = 0; previous < column; ++previous)
        {
            if (equalsAsciiInsensitive(name, roster.columns[previous]))
            {
                result.add(issue(
                    "roster.column.duplicate",
                    "columns[" + std::to_string(column) + "]",
                    ValidationSeverity::Error,
                    -1,
                    static_cast<int>(column)
                    ));
                break;
            }
        }
    }

    if (roster.columnWidths.size() > roster.columns.size())
    {
        result.add(issue(
            "roster.column_widths.invalid_count",
            "columnWidths"
            ));
    }

    if (roster.rows.size() > RosterValidator::MaximumRows)
    {
        result.add(issue(
            "roster.rows.too_many",
            "rows"
            ));
    }

    for (std::size_t rowIndex = 0; rowIndex < roster.rows.size(); ++rowIndex)
    {
        const std::vector<std::string>& row = roster.rows[rowIndex];
        if (row.size() > roster.columns.size())
        {
            result.add(issue(
                "roster.row.too_many_cells",
                "rows[" + std::to_string(rowIndex) + "]",
                ValidationSeverity::Error,
                static_cast<int>(rowIndex)
                ));
        }

        for (std::size_t column = 0; column < row.size(); ++column)
        {
            const std::string field = column < roster.columns.size()
                ? "rows[" + std::to_string(rowIndex) + "]."
                    + roster.columns[column]
                : "rows[" + std::to_string(rowIndex) + "].cells["
                    + std::to_string(column) + "]";
            addTextLengthIssue(
                result,
                row[column],
                0,
                RosterValidator::MaximumCellLength,
                field,
                static_cast<int>(rowIndex),
                static_cast<int>(column)
                );
        }

        if (!rowHasData(row))
        {
            continue;
        }

        addRequiredNameIssue(
            result,
            row,
            static_cast<int>(rowIndex),
            englishColumn,
            "rows[" + std::to_string(rowIndex) + "].English"
            );
        addRequiredNameIssue(
            result,
            row,
            static_cast<int>(rowIndex),
            koreanColumn,
            "rows[" + std::to_string(rowIndex) + "].Korean"
            );

        if (englishColumn >= 0
            && static_cast<std::size_t>(englishColumn) < row.size()
            && !trimAsciiWhitespace(
                row[static_cast<std::size_t>(englishColumn)]).empty())
        {
            addEnglishNameIssues(
                result,
                row[static_cast<std::size_t>(englishColumn)],
                static_cast<int>(rowIndex),
                englishColumn,
                "rows[" + std::to_string(rowIndex) + "].English"
                );
        }

        if (koreanColumn >= 0
            && static_cast<std::size_t>(koreanColumn) < row.size()
            && !trimAsciiWhitespace(
                row[static_cast<std::size_t>(koreanColumn)]).empty())
        {
            addKoreanNameIssues(
                result,
                row[static_cast<std::size_t>(koreanColumn)],
                static_cast<int>(rowIndex),
                koreanColumn,
                "rows[" + std::to_string(rowIndex) + "].Korean",
                allowQuestionableKoreanNameLengths
                );
        }
    }

    if (englishColumn >= 0 && koreanColumn >= 0)
    {
        std::map<std::string, std::vector<int>> rowsByPair;
        for (std::size_t rowIndex = 0; rowIndex < roster.rows.size(); ++rowIndex)
        {
            const std::string key = namePairKey(
                roster.rows[rowIndex], englishColumn, koreanColumn);
            if (!key.empty())
            {
                rowsByPair[key].push_back(static_cast<int>(rowIndex));
            }
        }

        for (const auto& [key, rows] : rowsByPair)
        {
            static_cast<void>(key);
            if (rows.size() < 2)
            {
                continue;
            }

            for (const int row : rows)
            {
                result.add(issue(
                    "student_name.duplicate_pair",
                    "English",
                    ValidationSeverity::Error,
                    row,
                    englishColumn
                    ));
                result.add(issue(
                    "student_name.duplicate_pair",
                    "Korean",
                    ValidationSeverity::Error,
                    row,
                    koreanColumn
                    ));
            }
        }
    }

    return result;
}

} // namespace classmngr::engine

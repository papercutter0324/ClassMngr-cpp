#include "classmngr/engine/speaking_evaluation_validator.h"

#include "classmngr/engine/student_name.h"

#include <algorithm>
#include <array>
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
constexpr int EnglishNameColumn = 1;
constexpr int KoreanNameColumn = 2;
constexpr int FirstScoringColumn = 3;
constexpr int LastScoringColumn = 8;
constexpr int CommentsColumn = 9;
constexpr int NotesColumn = 10;

constexpr std::array<std::string_view, SpeakingEvaluationValidator::MaximumColumns>
    ColumnHeaders{
        "",
        "English Name",
        "Korean Name",
        "Grammar",
        "Pronunciation",
        "Fluency",
        "Manner",
        "Content",
        "Overall Effort",
        "Comments",
        "Notes"
    };

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

std::size_t utf8Length(std::string_view value)
{
    const DecodedText decoded = decode(value);
    if (!decoded.valid)
    {
        return value.size();
    }

    return decoded.codePoints.size();
}

bool isAsciiLetter(unsigned codePoint)
{
    return (codePoint >= 'A' && codePoint <= 'Z')
        || (codePoint >= 'a' && codePoint <= 'z');
}

std::string fieldName(int row, int column)
{
    return "rows[" + std::to_string(row) + "]."
        + std::string(ColumnHeaders[static_cast<std::size_t>(column)]);
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

bool validKoreanShape(const std::string& value)
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

bool hasValueAt(
    const SpeakingEvaluationRow& row,
    int column
    )
{
    return column >= 0
        && static_cast<std::size_t>(column) < row.size()
        && !trimAsciiWhitespace(row[static_cast<std::size_t>(column)]).empty();
}

bool rowHasEditableData(const SpeakingEvaluationRow& row)
{
    for (std::size_t column = 1; column < row.size(); ++column)
    {
        if (!trimAsciiWhitespace(row[column]).empty())
        {
            return true;
        }
    }

    return false;
}

void addRequiredNameIssue(
    ValidationResult& result,
    const SpeakingEvaluationRow& row,
    int rowIndex,
    int column
    )
{
    if (hasValueAt(row, column))
    {
        return;
    }

    result.add(issue(
        "speaking_evaluation.student_name.required",
        fieldName(rowIndex, column),
        ValidationSeverity::Error,
        rowIndex,
        column
        ));
}

void addStringEnumIssue(
    ValidationResult& result,
    const std::string& value,
    std::string field,
    int row,
    int column
    )
{
    for (const std::string_view allowed : {"A+", "A", "B+", "B", "C"})
    {
        if (value == allowed)
        {
            return;
        }
    }

    result.add(issue(
        "validation.enum.invalid_value",
        std::move(field),
        ValidationSeverity::Error,
        row,
        column
        ));
}

std::string namePairKey(const SpeakingEvaluationRow& row)
{
    if (!hasValueAt(row, EnglishNameColumn)
        || !hasValueAt(row, KoreanNameColumn))
    {
        return {};
    }

    const std::string english = trimAsciiWhitespace(
        row[static_cast<std::size_t>(EnglishNameColumn)]);
    const std::string korean = trimAsciiWhitespace(
        row[static_cast<std::size_t>(KoreanNameColumn)]);

    std::string key;
    key.reserve(english.size() + korean.size() + 1);
    key += english;
    key.push_back('\x1f');
    key += korean;
    return key;
}
} // namespace

std::string SpeakingEvaluationValidator::normalizedScore(std::string_view value)
{
    const std::string trimmed = trimAsciiWhitespace(value);
    if (trimmed.empty())
    {
        return {};
    }

    std::string compact;
    compact.reserve(trimmed.size());
    for (const unsigned char character : trimmed)
    {
        if (std::isspace(character) != 0)
        {
            continue;
        }

        compact.push_back(static_cast<char>(std::toupper(character)));
    }

    if (compact == "1" || compact == "\xe3\x85\x8a")
    {
        return "C";
    }
    if (compact == "2" || compact == "\xe3\x85\xa0")
    {
        return "B";
    }
    if (compact == "3" || compact == "\xe3\x85\xa0+")
    {
        return "B+";
    }
    if (compact == "4" || compact == "\xe3\x85\x81")
    {
        return "A";
    }
    if (compact == "5" || compact == "\xe3\x85\x81+")
    {
        return "A+";
    }

    if (compact == "A+" || compact == "A" || compact == "B+"
        || compact == "B" || compact == "C")
    {
        return compact;
    }

    // A malformed score must remain recognizable to validation; never turn
    // it into another value merely because it contains whitespace.
    return trimmed;
}

SpeakingEvaluationRows SpeakingEvaluationValidator::normalized(
    const SpeakingEvaluationRows& rows
    )
{
    SpeakingEvaluationRows normalized;
    normalized.reserve(rows.size());
    for (const SpeakingEvaluationRow& sourceRow : rows)
    {
        SpeakingEvaluationRow row;
        row.reserve(sourceRow.size());
        for (std::size_t column = 0; column < sourceRow.size(); ++column)
        {
            const std::string& value = sourceRow[column];
            if (column >= MaximumColumns)
            {
                // Keep structural errors visible to validate() instead of
                // silently dropping pasted values outside the supported grid.
                row.push_back(value);
                continue;
            }

            if (column == static_cast<std::size_t>(EnglishNameColumn))
            {
                row.push_back(StudentNameService::normalizeEnglish(value));
            }
            else if (column == static_cast<std::size_t>(KoreanNameColumn))
            {
                row.push_back(StudentNameService::normalizeKorean(value));
            }
            else if (column >= static_cast<std::size_t>(FirstScoringColumn)
                     && column <= static_cast<std::size_t>(LastScoringColumn))
            {
                row.push_back(normalizedScore(value));
            }
            else if (column == 0)
            {
                row.push_back(trimAsciiWhitespace(value));
            }
            else
            {
                // Comments and notes intentionally retain their source text.
                row.push_back(value);
            }
        }
        normalized.push_back(std::move(row));
    }

    return normalized;
}

ValidationResult SpeakingEvaluationValidator::validate(
    int classId,
    std::string_view evaluationName,
    const SpeakingEvaluationRows& rows,
    bool allowQuestionableKoreanNameLengths
    )
{
    ValidationResult result;
    const std::string normalizedName = trimAsciiWhitespace(evaluationName);

    if (classId <= 0)
    {
        result.add(issue(
            "speaking_evaluation.class_id.invalid",
            "classId"
            ));
    }
    addTextLengthIssue(
        result,
        normalizedName,
        1,
        MaximumEvaluationNameLength,
        "evaluationName"
        );

    if (rows.size() > MaximumRows)
    {
        result.add(issue(
            "speaking_evaluation.rows.too_many",
            "rows"
            ));
    }

    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const SpeakingEvaluationRow& row = rows[rowIndex];
        if (row.size() > MaximumColumns)
        {
            result.add(issue(
                "speaking_evaluation.row.too_many_cells",
                "rows[" + std::to_string(rowIndex) + "]",
                ValidationSeverity::Error,
                static_cast<int>(rowIndex)
                ));
        }

        if (!rowHasEditableData(row))
        {
            continue;
        }

        addRequiredNameIssue(
            result,
            row,
            static_cast<int>(rowIndex),
            EnglishNameColumn
            );
        addRequiredNameIssue(
            result,
            row,
            static_cast<int>(rowIndex),
            KoreanNameColumn
            );

        if (hasValueAt(row, EnglishNameColumn))
        {
            addEnglishNameIssues(
                result,
                row[static_cast<std::size_t>(EnglishNameColumn)],
                static_cast<int>(rowIndex),
                EnglishNameColumn,
                fieldName(static_cast<int>(rowIndex), EnglishNameColumn)
                );
        }
        if (hasValueAt(row, KoreanNameColumn))
        {
            addKoreanNameIssues(
                result,
                row[static_cast<std::size_t>(KoreanNameColumn)],
                static_cast<int>(rowIndex),
                KoreanNameColumn,
                fieldName(static_cast<int>(rowIndex), KoreanNameColumn),
                allowQuestionableKoreanNameLengths
                );
        }

        const std::size_t columnsToValidate = std::min(
            row.size(),
            MaximumColumns
            );
        for (std::size_t column = 0; column < columnsToValidate; ++column)
        {
            const std::string& value = row[column];
            const int columnIndex = static_cast<int>(column);
            const std::string field = fieldName(
                static_cast<int>(rowIndex),
                columnIndex
                );
            if (column >= static_cast<std::size_t>(FirstScoringColumn)
                && column <= static_cast<std::size_t>(LastScoringColumn)
                && !trimAsciiWhitespace(value).empty())
            {
                addStringEnumIssue(
                    result,
                    value,
                    field,
                    static_cast<int>(rowIndex),
                    columnIndex
                    );
            }
            else if (column == static_cast<std::size_t>(CommentsColumn))
            {
                addTextLengthIssue(
                    result,
                    value,
                    0,
                    CommentMaxLength,
                    field,
                    static_cast<int>(rowIndex),
                    columnIndex
                    );
            }
            else if (column == static_cast<std::size_t>(NotesColumn))
            {
                addTextLengthIssue(
                    result,
                    value,
                    0,
                    MaximumNotesLength,
                    field,
                    static_cast<int>(rowIndex),
                    columnIndex
                    );
            }
        }
    }

    std::map<std::string, std::vector<int>> rowsByPair;
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const std::string key = namePairKey(rows[rowIndex]);
        if (!key.empty())
        {
            rowsByPair[key].push_back(static_cast<int>(rowIndex));
        }
    }

    for (const auto& [key, duplicateRows] : rowsByPair)
    {
        static_cast<void>(key);
        if (duplicateRows.size() < 2)
        {
            continue;
        }

        for (const int row : duplicateRows)
        {
            result.add(issue(
                "student_name.duplicate_pair",
                "English Name",
                ValidationSeverity::Error,
                row,
                EnglishNameColumn
                ));
            result.add(issue(
                "student_name.duplicate_pair",
                "Korean Name",
                ValidationSeverity::Error,
                row,
                KoreanNameColumn
                ));
        }
    }

    return result;
}

} // namespace classmngr::engine

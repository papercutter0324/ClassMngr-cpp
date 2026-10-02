#pragma once

#include "next/domain/domain_types.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{

inline constexpr int SpeakingEvaluationRowCount = 25;
inline constexpr int SpeakingEvaluationColumnCount = 11;
inline constexpr std::size_t SpeakingEvaluationMaximumNameLength = 128;
inline constexpr std::size_t SpeakingEvaluationMaximumEnglishNameLength = 20;
inline constexpr std::size_t SpeakingEvaluationMaximumCommentLength = 450;
inline constexpr std::size_t SpeakingEvaluationMaximumNotesLength = 10000;

struct SpeakingEvaluationCellChange final
{
    int row = -1;
    int column = -1;

    friend bool operator==(
        const SpeakingEvaluationCellChange&,
        const SpeakingEvaluationCellChange&
        ) = default;
};

struct SpeakingEvaluationSnapshot final
{
    std::vector<std::vector<std::u16string>> rows;
    std::vector<SpeakingEvaluationCellChange> changedCells;

    friend bool operator==(
        const SpeakingEvaluationSnapshot&,
        const SpeakingEvaluationSnapshot&
        ) = default;
};

struct SpeakingEvaluationSaveRequest final
{
    Domain::ClassId classId;
    std::u16string evaluationName;
    SpeakingEvaluationSnapshot evaluation;
    bool allowQuestionableKoreanNameLengths = false;

    friend bool operator==(
        const SpeakingEvaluationSaveRequest&,
        const SpeakingEvaluationSaveRequest&
        ) = default;
};

enum class SpeakingEvaluationValidationSeverity
{
    Warning,
    Error
};

using SpeakingEvaluationValidationArgument = std::variant<
    std::int64_t,
    std::u16string,
    std::vector<int>,
    std::vector<std::u16string>
    >;

struct SpeakingEvaluationValidationIssue final
{
    std::string code;
    std::string field;
    int row = -1;
    int column = -1;
    SpeakingEvaluationValidationSeverity severity =
        SpeakingEvaluationValidationSeverity::Error;
    std::map<std::string, SpeakingEvaluationValidationArgument> arguments;

    friend bool operator==(
        const SpeakingEvaluationValidationIssue&,
        const SpeakingEvaluationValidationIssue&
        ) = default;
};

struct SpeakingEvaluationValidationResult final
{
    SpeakingEvaluationSaveRequest normalizedRequest;
    std::vector<SpeakingEvaluationValidationIssue> issues;

    [[nodiscard]] bool hasErrors() const noexcept
    {
        return std::ranges::any_of(
            issues,
            [](const SpeakingEvaluationValidationIssue& issue)
            {
                return issue.severity
                    == SpeakingEvaluationValidationSeverity::Error;
            }
            );
    }

    [[nodiscard]] bool hasWarnings() const noexcept
    {
        return std::ranges::any_of(
            issues,
            [](const SpeakingEvaluationValidationIssue& issue)
            {
                return issue.severity
                    == SpeakingEvaluationValidationSeverity::Warning;
            }
            );
    }
};

namespace SpeakingEvaluationValidationDetail
{

inline constexpr std::array<std::string_view, SpeakingEvaluationColumnCount>
    ColumnNames{
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

inline constexpr std::array<std::u16string_view, 5> ScoreValues{
    u"A+", u"A", u"B+", u"B", u"C"
};

inline constexpr std::array<int, 6> ScoreColumns{3, 4, 5, 6, 7, 8};

inline bool isHangulSyllable(const char16_t character) noexcept
{
    return character >= 0xAC00 && character <= 0xD7A3;
}

// QString::trimmed(), QChar::isSpace(), and the regular-expression \s rules
// used by the legacy validator all operate on UTF-16 code units here.
inline bool isUnicodeSpace(const char16_t character) noexcept
{
    return (character >= 0x0009 && character <= 0x000D)
        || character == 0x0020
        || character == 0x0085
        || character == 0x00A0
        || character == 0x1680
        || (character >= 0x2000 && character <= 0x200A)
        || character == 0x2028
        || character == 0x2029
        || character == 0x202F
        || character == 0x205F
        || character == 0x3000;
}

inline std::u16string trim(std::u16string_view value)
{
    std::size_t first = 0;
    while (first < value.size() && isUnicodeSpace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isUnicodeSpace(value[last - 1]))
    {
        --last;
    }

    return std::u16string(value.substr(first, last - first));
}

inline std::u16string_view cell(
    const std::vector<std::u16string>& row,
    const int column
    ) noexcept
{
    if (column < 0 || static_cast<std::size_t>(column) >= row.size())
    {
        return {};
    }
    return row[static_cast<std::size_t>(column)];
}

inline std::string fieldName(const int row, const int column)
{
    return "rows[" + std::to_string(row) + "]."
        + std::string(ColumnNames[static_cast<std::size_t>(column)]);
}

inline bool asciiLetter(const char16_t character) noexcept
{
    return (character >= u'A' && character <= u'Z')
        || (character >= u'a' && character <= u'z');
}

inline char asciiUpper(char character) noexcept
{
    if (character >= 'a' && character <= 'z')
    {
        character = static_cast<char>(character - ('a' - 'A'));
    }
    return character;
}

inline char asciiLower(char character) noexcept
{
    if (character >= 'A' && character <= 'Z')
    {
        character = static_cast<char>(character + ('a' - 'A'));
    }
    return character;
}

inline bool containsNonAscii(std::u16string_view value) noexcept
{
    return std::ranges::any_of(
        value,
        [](const char16_t character)
        {
            return character > 0x7F;
        }
        );
}

inline bool containsInvalidEnglishCharacters(
    std::u16string_view value
    ) noexcept
{
    for (const char16_t character : value)
    {
        if (asciiLetter(character)
            || character == u'.'
            || character == u'-'
            || isUnicodeSpace(character)
            || character > 0x7F)
        {
            continue;
        }
        return true;
    }
    return false;
}

inline std::string narrowAscii(std::u16string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char16_t character : value)
    {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

inline std::string simplifiedAscii(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;
    for (const char character : value)
    {
        if (character == ' ' || character == '\t' || character == '\n'
            || character == '\r' || character == '\f' || character == '\v')
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
    return result;
}

inline std::u16string normalizeEnglishName(std::u16string_view value)
{
    const std::u16string trimmed = trim(value);
    if (trimmed.empty())
    {
        return {};
    }

    // Preserve malformed and Unicode input so validation can report it in the
    // original form rather than silently changing it into a valid name.
    if (containsNonAscii(value) || containsInvalidEnglishCharacters(value))
    {
        return trimmed;
    }

    std::string filtered;
    filtered.reserve(value.size());
    for (const char16_t character : value)
    {
        if (asciiLetter(character)
            || character == u'.'
            || character == u'-')
        {
            filtered.push_back(static_cast<char>(character));
        }
        else if (isUnicodeSpace(character))
        {
            filtered.push_back(' ');
        }
    }

    std::string cleaned = simplifiedAscii(filtered);
    cleaned = std::regex_replace(cleaned, std::regex(R"(\s*-\s*)"), "-");
    cleaned = std::regex_replace(cleaned, std::regex(R"(-{2,})"), "-");
    cleaned = std::regex_replace(cleaned, std::regex(R"(\.{2,})"), ".");
    cleaned = std::regex_replace(cleaned, std::regex(R"(\s*\.\s*)"), ".");
    cleaned = std::regex_replace(
        cleaned,
        std::regex(R"(\b([A-Za-z])[.-]+-?[.-]*([A-Za-z])\b)"),
        "$1.$2"
        );

    std::string result;
    std::string token;
    char previousSeparator = '\0';
    const auto flushToken = [&result, &token, &previousSeparator]()
    {
        if (token.empty())
        {
            return;
        }

        for (char& character : token)
        {
            character = asciiLower(character);
        }
        if (result.empty()
            || previousSeparator == ' '
            || previousSeparator == '.')
        {
            token.front() = asciiUpper(token.front());
        }
        result += token;
        token.clear();
    };

    for (const char character : cleaned)
    {
        if (character == ' ' || character == '.' || character == '-')
        {
            flushToken();
            result.push_back(character);
            previousSeparator = character;
        }
        else
        {
            token.push_back(character);
        }
    }
    flushToken();
    result = std::regex_replace(
        result,
        std::regex(R"(\b([A-Za-z])\. ?([A-Za-z])\.)"),
        "$1.$2."
        );

    std::u16string normalized;
    normalized.reserve(result.size());
    for (const char character : result)
    {
        normalized.push_back(static_cast<char16_t>(character));
    }
    return trim(normalized);
}

inline bool isValidKoreanNameCharacters(std::u16string_view value)
{
    if (trim(value).empty())
    {
        return true;
    }

    std::size_t position = 0;
    while (position < value.size()
           && (isUnicodeSpace(value[position])
               || isHangulSyllable(value[position])))
    {
        ++position;
    }
    if (position == 0)
    {
        return false;
    }

    while (position < value.size() && isUnicodeSpace(value[position]))
    {
        ++position;
    }

    if (position + 2 < value.size()
        && value[position] == u'('
        && asciiLetter(value[position + 1])
        && value[position + 2] == u')')
    {
        position += 3;
    }

    while (position < value.size() && isUnicodeSpace(value[position]))
    {
        ++position;
    }
    return position == value.size();
}

struct KoreanSuffix final
{
    std::size_t start = 0;
    char16_t letter = 0;
};

inline std::optional<KoreanSuffix> trailingKoreanSuffix(
    std::u16string_view value
    ) noexcept
{
    std::size_t end = value.size();
    while (end > 0 && isUnicodeSpace(value[end - 1]))
    {
        --end;
    }

    if (end >= 3
        && value[end - 3] == u'('
        && asciiLetter(value[end - 2])
        && value[end - 1] == u')')
    {
        return KoreanSuffix{end - 3, value[end - 2]};
    }
    return std::nullopt;
}

inline std::u16string normalizeKoreanName(std::u16string_view value)
{
    const std::u16string trimmed = trim(value);
    if (!isValidKoreanNameCharacters(value))
    {
        return trimmed;
    }

    const auto suffix = trailingKoreanSuffix(value);
    const std::u16string_view source = suffix
        ? value.substr(0, suffix->start)
        : value;

    std::u16string normalized;
    normalized.reserve(source.size() + (suffix ? 3U : 0U));
    for (const char16_t character : source)
    {
        if (isHangulSyllable(character))
        {
            normalized.push_back(character);
        }
    }

    if (!normalized.empty() && suffix)
    {
        normalized.push_back(u'(');
        normalized.push_back(static_cast<char16_t>(
            asciiUpper(static_cast<char>(suffix->letter))
            ));
        normalized.push_back(u')');
    }
    return normalized;
}

inline std::u16string normalizeScore(std::u16string_view value)
{
    const std::u16string trimmed = trim(value);
    if (trimmed.empty())
    {
        return {};
    }

    std::u16string compact;
    compact.reserve(trimmed.size());
    for (const char16_t character : trimmed)
    {
        if (isUnicodeSpace(character))
        {
            continue;
        }
        compact.push_back(character <= 0x7F
                ? static_cast<char16_t>(asciiUpper(static_cast<char>(character)))
                : character);
    }

    const char16_t koreanC = 0x314A;
    const char16_t koreanB = 0x3160;
    const char16_t koreanA = 0x3141;
    if (compact == u"1" || compact == std::u16string(1, koreanC))
    {
        return u"C";
    }
    if (compact == u"2" || compact == std::u16string(1, koreanB))
    {
        return u"B";
    }
    if (compact == u"3"
        || compact == std::u16string(1, koreanB) + u"+")
    {
        return u"B+";
    }
    if (compact == u"4" || compact == std::u16string(1, koreanA))
    {
        return u"A";
    }
    if (compact == u"5"
        || compact == std::u16string(1, koreanA) + u"+")
    {
        return u"A+";
    }

    for (const std::u16string_view score : ScoreValues)
    {
        if (compact == score)
        {
            return std::u16string(score);
        }
    }

    // Malformed scores stay recognizable in the issue and saved draft.
    return trimmed;
}

inline bool isScoringColumn(const int column) noexcept
{
    return std::ranges::find(ScoreColumns, column) != ScoreColumns.end();
}

inline bool rowHasEditableData(
    const std::vector<std::u16string>& row
    )
{
    for (std::size_t column = 1; column < row.size(); ++column)
    {
        if (!trim(row[column]).empty())
        {
            return true;
        }
    }
    return false;
}

inline void appendIssue(
    SpeakingEvaluationValidationResult& result,
    std::string code,
    std::string field,
    const int row = -1,
    const int column = -1,
    const SpeakingEvaluationValidationSeverity severity =
        SpeakingEvaluationValidationSeverity::Error,
    std::map<std::string, SpeakingEvaluationValidationArgument> arguments = {}
    )
{
    result.issues.push_back({
        .code = std::move(code),
        .field = std::move(field),
        .row = row,
        .column = column,
        .severity = severity,
        .arguments = std::move(arguments)
    });
}

inline std::map<std::string, SpeakingEvaluationValidationArgument>
lengthArguments(
    const std::size_t length,
    const std::size_t minimum,
    const std::size_t maximum
    )
{
    return {
        {"length", static_cast<std::int64_t>(length)},
        {"maximum", static_cast<std::int64_t>(maximum)},
        {"minimum", static_cast<std::int64_t>(minimum)}
    };
}

inline std::size_t koreanBaseNameLength(std::u16string_view value)
{
    const std::u16string normalized = normalizeKoreanName(value);
    if (const auto suffix = trailingKoreanSuffix(normalized);
        suffix && suffix->letter >= u'A' && suffix->letter <= u'Z')
    {
        return suffix->start;
    }
    return normalized.size();
}

inline void validateEnglishName(
    SpeakingEvaluationValidationResult& result,
    const std::u16string_view value,
    const int row,
    const int column
    )
{
    const std::string field = fieldName(row, column);
    if (value.size() > SpeakingEvaluationMaximumEnglishNameLength)
    {
        appendIssue(
            result,
            "student_name.english.too_long",
            field,
            row,
            column,
            SpeakingEvaluationValidationSeverity::Error,
            {{"maximumLength", static_cast<std::int64_t>(
                 SpeakingEvaluationMaximumEnglishNameLength)}}
            );
    }
    if (containsNonAscii(value))
    {
        appendIssue(
            result,
            "student_name.english.non_ascii",
            field,
            row,
            column
            );
    }
    if (containsInvalidEnglishCharacters(value))
    {
        appendIssue(
            result,
            "student_name.english.invalid_characters",
            field,
            row,
            column
            );
    }
}

inline void validateKoreanName(
    SpeakingEvaluationValidationResult& result,
    const std::u16string_view value,
    const int row,
    const int column,
    const bool allowQuestionableLength
    )
{
    const std::string field = fieldName(row, column);
    if (!isValidKoreanNameCharacters(value))
    {
        appendIssue(
            result,
            "student_name.korean.invalid_characters",
            field,
            row,
            column
            );
    }

    const std::size_t length = koreanBaseNameLength(value);
    if (length == 0 || length == 3)
    {
        return;
    }

    if (length <= 1)
    {
        appendIssue(
            result,
            "student_name.korean.too_short",
            field,
            row,
            column,
            allowQuestionableLength
                ? SpeakingEvaluationValidationSeverity::Warning
                : SpeakingEvaluationValidationSeverity::Error
            );
    }
    else if (length >= 5)
    {
        appendIssue(
            result,
            "student_name.korean.too_long",
            field,
            row,
            column,
            allowQuestionableLength
                ? SpeakingEvaluationValidationSeverity::Warning
                : SpeakingEvaluationValidationSeverity::Error
            );
    }
    else
    {
        appendIssue(
            result,
            "student_name.korean.unusual_length",
            field,
            row,
            column,
            SpeakingEvaluationValidationSeverity::Warning
            );
    }
}

inline void appendDuplicateNameIssues(
    SpeakingEvaluationValidationResult& result
    )
{
    struct PairGroup final
    {
        std::u16string english;
        std::u16string korean;
        std::vector<int> rows;
    };

    std::vector<PairGroup> groups;
    for (std::size_t rowIndex = 0;
         rowIndex < result.normalizedRequest.evaluation.rows.size();
         ++rowIndex)
    {
        const auto& row = result.normalizedRequest.evaluation.rows[rowIndex];
        const std::u16string english = trim(cell(row, 1));
        const std::u16string korean = trim(cell(row, 2));
        if (english.empty() || korean.empty())
        {
            continue;
        }

        auto group = std::ranges::find_if(
            groups,
            [&english, &korean](const PairGroup& candidate)
            {
                return candidate.english == english
                    && candidate.korean == korean;
            }
            );
        if (group == groups.end())
        {
            groups.push_back({english, korean, {
                static_cast<int>(rowIndex)
            }});
        }
        else
        {
            group->rows.push_back(static_cast<int>(rowIndex));
        }
    }

    for (const PairGroup& group : groups)
    {
        if (group.rows.size() < 2)
        {
            continue;
        }

        const std::vector<int> duplicateRows = group.rows;
        for (const int row : group.rows)
        {
            for (const int column : {1, 2})
            {
                appendIssue(
                    result,
                    "student_name.duplicate_pair",
                    std::string(ColumnNames[static_cast<std::size_t>(column)]),
                    row,
                    column,
                    SpeakingEvaluationValidationSeverity::Error,
                    {{"duplicateRows", duplicateRows}}
                    );
            }
        }
    }
}

} // namespace SpeakingEvaluationValidationDetail

// Normalizes the request before checking its content. The save use case owns
// matrix-shape and changed-cell checks; the content rules below mirror the
// legacy speaking-evaluation validator without importing its Qt types.
[[nodiscard]] inline SpeakingEvaluationValidationResult
validateAndNormalizeSpeakingEvaluation(
    const SpeakingEvaluationSaveRequest& request
    )
{
    using namespace SpeakingEvaluationValidationDetail;

    SpeakingEvaluationSaveRequest normalized = request;
    normalized.evaluationName = trim(request.evaluationName);

    for (auto& row : normalized.evaluation.rows)
    {
        for (std::size_t column = 0;
             column < row.size()
             && column < static_cast<std::size_t>(SpeakingEvaluationColumnCount);
             ++column)
        {
            switch (column)
            {
            case 0:
                row[column] = trim(row[column]);
                break;
            case 1:
                row[column] = normalizeEnglishName(row[column]);
                break;
            case 2:
                row[column] = normalizeKoreanName(row[column]);
                break;
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                row[column] = normalizeScore(row[column]);
                break;
            default:
                break;
            }
        }
    }

    SpeakingEvaluationValidationResult result{
        .normalizedRequest = std::move(normalized),
        .issues = {}
    };

    const std::string& classId = result.normalizedRequest.classId.value();
    int parsedClassId = 0;
    const auto [classIdEnd, classIdError] = std::from_chars(
        classId.data(),
        classId.data() + classId.size(),
        parsedClassId
        );
    if (classIdError != std::errc{}
        || classIdEnd != classId.data() + classId.size()
        || parsedClassId <= 0)
    {
        appendIssue(
            result,
            "speaking_evaluation.class_id.invalid",
            "classId",
            -1,
            -1,
            SpeakingEvaluationValidationSeverity::Error,
            {{"value", static_cast<std::int64_t>(parsedClassId)}}
            );
    }

    const std::u16string& evaluationName =
        result.normalizedRequest.evaluationName;
    if (evaluationName.empty()
        || evaluationName.size() > SpeakingEvaluationMaximumNameLength)
    {
        appendIssue(
            result,
            "validation.length.out_of_bounds",
            "evaluationName",
            -1,
            -1,
            SpeakingEvaluationValidationSeverity::Error,
            lengthArguments(
                evaluationName.size(),
                1,
                SpeakingEvaluationMaximumNameLength
                )
            );
    }

    const auto& rows = result.normalizedRequest.evaluation.rows;
    if (rows.size() > static_cast<std::size_t>(SpeakingEvaluationRowCount))
    {
        appendIssue(
            result,
            "speaking_evaluation.rows.too_many",
            "rows",
            -1,
            -1,
            SpeakingEvaluationValidationSeverity::Error,
            {{"maximumRows", static_cast<std::int64_t>(
                 SpeakingEvaluationRowCount)},
             {"rowCount", static_cast<std::int64_t>(rows.size())}}
            );
    }

    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const auto& row = rows[rowIndex];
        const int rowNumber = static_cast<int>(rowIndex);
        if (row.size()
            > static_cast<std::size_t>(SpeakingEvaluationColumnCount))
        {
            appendIssue(
                result,
                "speaking_evaluation.row.too_many_cells",
                "rows[" + std::to_string(rowNumber) + "]",
                rowNumber,
                -1,
                SpeakingEvaluationValidationSeverity::Error,
                {{"cellCount", static_cast<std::int64_t>(row.size())},
                 {"columnCount", static_cast<std::int64_t>(
                      SpeakingEvaluationColumnCount)}}
                );
        }

        if (!rowHasEditableData(row))
        {
            continue;
        }

        for (const int column : {1, 2})
        {
            if (trim(cell(row, column)).empty())
            {
                appendIssue(
                    result,
                    "speaking_evaluation.student_name.required",
                    fieldName(rowNumber, column),
                    rowNumber,
                    column,
                    SpeakingEvaluationValidationSeverity::Error,
                    {{"column", std::u16string(
                         ColumnNames[static_cast<std::size_t>(column)].begin(),
                         ColumnNames[static_cast<std::size_t>(column)].end())}}
                    );
            }
        }

        const std::u16string_view english = cell(row, 1);
        if (!trim(english).empty())
        {
            validateEnglishName(result, english, rowNumber, 1);
        }
        const std::u16string_view korean = cell(row, 2);
        if (!trim(korean).empty())
        {
            validateKoreanName(
                result,
                korean,
                rowNumber,
                2,
                request.allowQuestionableKoreanNameLengths
                );
        }

        const std::size_t validatedColumns = std::min(
            row.size(),
            static_cast<std::size_t>(SpeakingEvaluationColumnCount)
            );
        for (std::size_t column = 0; column < validatedColumns; ++column)
        {
            const std::u16string_view value = row[column];
            const int columnNumber = static_cast<int>(column);
            const std::string field = fieldName(rowNumber, columnNumber);
            if (isScoringColumn(columnNumber) && !trim(value).empty())
            {
                const bool allowed = std::ranges::any_of(
                    ScoreValues,
                    [value](const std::u16string_view score)
                    {
                        return value == score;
                    }
                    );
                if (!allowed)
                {
                    appendIssue(
                        result,
                        "validation.enum.invalid_value",
                        field,
                        rowNumber,
                        columnNumber,
                        SpeakingEvaluationValidationSeverity::Error,
                        {{"allowedValues", std::vector<std::u16string>{
                             u"A+", u"A", u"B+", u"B", u"C"}},
                         {"value", std::u16string(value)}}
                        );
                }
            }
            else if (columnNumber == 9
                     && value.size() > SpeakingEvaluationMaximumCommentLength)
            {
                appendIssue(
                    result,
                    "validation.length.out_of_bounds",
                    field,
                    rowNumber,
                    columnNumber,
                    SpeakingEvaluationValidationSeverity::Error,
                    lengthArguments(
                        value.size(),
                        0,
                        SpeakingEvaluationMaximumCommentLength
                        )
                    );
            }
            else if (columnNumber == 10
                     && value.size() > SpeakingEvaluationMaximumNotesLength)
            {
                appendIssue(
                    result,
                    "validation.length.out_of_bounds",
                    field,
                    rowNumber,
                    columnNumber,
                    SpeakingEvaluationValidationSeverity::Error,
                    lengthArguments(
                        value.size(),
                        0,
                        SpeakingEvaluationMaximumNotesLength
                        )
                    );
            }
        }
    }

    appendDuplicateNameIssues(result);
    return result;
}

} // namespace ClassMngr::Next::Application

#pragma once

#include "next/application/roster_custom_column_name_policy.h"
#include "next/application/roster_row_availability.h"
#include "next/application/roster_snapshot.h"
#include "next/application/speaking_evaluation_validation.h"
#include "next/domain/student_name_pair.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Application
{

using RosterSaveCaseInsensitiveEquals =
    std::function<bool(std::u16string_view, std::u16string_view)>;

enum class RosterSaveIssueSeverity { Warning, Error };

enum class RosterSaveIssueCode
{
    ValidationLengthOutOfBounds,
    StudentNameEnglishTooLong,
    StudentNameEnglishNonAscii,
    StudentNameEnglishInvalidCharacters,
    StudentNameKoreanInvalidCharacters,
    StudentNameKoreanTooShort,
    StudentNameKoreanTooLong,
    StudentNameKoreanUnusualLength,
    RosterColumnRequired,
    RosterColumnDuplicate,
    RosterColumnWidthsInvalidCount,
    RosterRowsTooMany,
    RosterRowTooManyCells,
    RosterStudentNameRequired,
    StudentNameDuplicatePair
};

[[nodiscard]] inline constexpr std::string_view rosterSaveIssueCodeKey(
    const RosterSaveIssueCode code) noexcept
{
    switch (code)
    {
    case RosterSaveIssueCode::ValidationLengthOutOfBounds:
        return "validation.length.out_of_bounds";
    case RosterSaveIssueCode::StudentNameEnglishTooLong:
        return "student_name.english.too_long";
    case RosterSaveIssueCode::StudentNameEnglishNonAscii:
        return "student_name.english.non_ascii";
    case RosterSaveIssueCode::StudentNameEnglishInvalidCharacters:
        return "student_name.english.invalid_characters";
    case RosterSaveIssueCode::StudentNameKoreanInvalidCharacters:
        return "student_name.korean.invalid_characters";
    case RosterSaveIssueCode::StudentNameKoreanTooShort:
        return "student_name.korean.too_short";
    case RosterSaveIssueCode::StudentNameKoreanTooLong:
        return "student_name.korean.too_long";
    case RosterSaveIssueCode::StudentNameKoreanUnusualLength:
        return "student_name.korean.unusual_length";
    case RosterSaveIssueCode::RosterColumnRequired:
        return "roster.column.required";
    case RosterSaveIssueCode::RosterColumnDuplicate:
        return "roster.column.duplicate";
    case RosterSaveIssueCode::RosterColumnWidthsInvalidCount:
        return "roster.column_widths.invalid_count";
    case RosterSaveIssueCode::RosterRowsTooMany:
        return "roster.rows.too_many";
    case RosterSaveIssueCode::RosterRowTooManyCells:
        return "roster.row.too_many_cells";
    case RosterSaveIssueCode::RosterStudentNameRequired:
        return "roster.student_name.required";
    case RosterSaveIssueCode::StudentNameDuplicatePair:
        return "student_name.duplicate_pair";
    }
    return {};
}

using RosterSaveIssueArgument =
    std::variant<
        std::int64_t,
        std::u16string,
        std::vector<Domain::DuplicateStudentNamePairRowIndex>
        >;

class RosterSaveIssueRowIndex final
{
public:
    constexpr RosterSaveIssueRowIndex() noexcept = default;

    explicit constexpr RosterSaveIssueRowIndex(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const RosterSaveIssueRowIndex&,
        const RosterSaveIssueRowIndex&
        ) noexcept = default;

private:
    int m_value = -1;
};

class RosterSaveIssueColumnIndex final
{
public:
    constexpr RosterSaveIssueColumnIndex() noexcept = default;

    explicit constexpr RosterSaveIssueColumnIndex(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const RosterSaveIssueColumnIndex&,
        const RosterSaveIssueColumnIndex&
        ) noexcept = default;

private:
    int m_value = -1;
};

struct RosterSaveIssue final
{
    RosterSaveIssueCode code;
    std::u16string field;
    RosterSaveIssueRowIndex row;
    RosterSaveIssueColumnIndex column;
    RosterSaveIssueSeverity severity = RosterSaveIssueSeverity::Error;
    std::map<std::string, RosterSaveIssueArgument> arguments;

    friend bool operator==(const RosterSaveIssue&, const RosterSaveIssue&) = default;
};

struct RosterSavePreparation final
{
    RosterSnapshot roster;
    std::vector<RosterSaveIssue> issues;

    [[nodiscard]] bool hasErrors() const noexcept
    {
        return std::ranges::any_of(issues, [](const RosterSaveIssue& issue)
        {
            return issue.severity == RosterSaveIssueSeverity::Error;
        });
    }
};

inline constexpr std::size_t RosterSaveMaximumRows = RosterModeledRowCount;
inline constexpr std::size_t RosterSaveMaximumColumnNameLength =
    RosterMaximumColumnNameLength;
inline constexpr std::size_t RosterSaveMaximumCellLength = 10000;
inline constexpr std::array<std::u16string_view, 6> RosterSaveRequiredColumns{
    u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"
};

namespace RosterSavePreparationDetail
{
namespace Names = SpeakingEvaluationValidationDetail;

// The legacy port constructs each string with QString::fromUtf16(): a leading
// BOM is consumed, and a reverse BOM switches the byte order of every unit.
inline std::u16string snapshotText(std::u16string_view value)
{
    if (value.empty())
        return {};
    const bool swap = value.front() == 0xfffe;
    if (swap || value.front() == 0xfeff)
        value.remove_prefix(1);
    std::u16string result(value);
    if (swap)
        for (auto& unit : result)
            unit = static_cast<char16_t>((unit >> 8) | (unit << 8));
    return result;
}

inline bool regexSpace(const char16_t value) noexcept
{
    // QRegularExpression uses PCRE's ASCII \s without UnicodePropertiesOption.
    return (value >= u'\t' && value <= u'\r') || value == u' ';
}

inline bool validKoreanCharacters(const std::u16string_view value)
{
    if (trimQtWhitespace(value).empty())
        return true;
    std::size_t position = 0;
    while (position < value.size()
           && (regexSpace(value[position]) || Names::isHangulSyllable(value[position])))
        ++position;
    if (position == 0)
        return false;
    if (position + 2 < value.size() && value[position] == u'('
        && Names::asciiLetter(value[position + 1]) && value[position + 2] == u')')
        position += 3;
    while (position < value.size() && regexSpace(value[position]))
        ++position;
    return position == value.size();
}

inline std::u16string normalizeKoreanName(const std::u16string_view value)
{
    return validKoreanCharacters(value)
        ? Names::normalizeKoreanName(value) : trimQtWhitespace(value);
}

inline std::size_t koreanBaseNameLength(const std::u16string_view value)
{
    const auto normalized = normalizeKoreanName(value);
    if (const auto suffix = Names::trailingKoreanSuffix(normalized);
        suffix && suffix->letter >= u'A' && suffix->letter <= u'Z')
        return suffix->start;
    return normalized.size();
}

inline std::u16string number(const std::size_t value)
{
    const std::string ascii = std::to_string(value);
    return {ascii.begin(), ascii.end()};
}

inline int columnIndex(const std::vector<std::u16string>& columns,
                       const std::u16string_view name,
                       const RosterSaveCaseInsensitiveEquals& equals)
{
    for (std::size_t index = 0; index < columns.size(); ++index)
    {
        if (equals(columns[index], name))
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

inline void lengthIssue(RosterSavePreparation& result, const std::u16string_view value,
                        const std::size_t minimum, const std::size_t maximum,
                        const std::u16string& field, const int row, const int column)
{
    if (value.size() < minimum || value.size() > maximum)
    {
        result.issues.push_back({
            .code = RosterSaveIssueCode::ValidationLengthOutOfBounds, .field = field,
            .row = RosterSaveIssueRowIndex{row},
            .column = RosterSaveIssueColumnIndex{column},
            .arguments = {{"length", static_cast<std::int64_t>(value.size())},
                          {"minimum", static_cast<std::int64_t>(minimum)},
                          {"maximum", static_cast<std::int64_t>(maximum)}}
        });
    }
}

inline void nameIssues(RosterSavePreparation& result, const std::u16string_view value,
                       const std::u16string& field, const int row, const int column,
                       const bool korean, const bool allowQuestionableLengths)
{
    const auto append = [&](const RosterSaveIssueCode code,
                            const RosterSaveIssueSeverity severity = RosterSaveIssueSeverity::Error,
                            std::map<std::string, RosterSaveIssueArgument> arguments = {})
    {
        result.issues.push_back({
            code,
            field,
            RosterSaveIssueRowIndex{row},
            RosterSaveIssueColumnIndex{column},
            severity,
            std::move(arguments)
        });
    };
    if (!korean)
    {
        if (value.size() > SpeakingEvaluationMaximumEnglishNameLength)
        {
            append(RosterSaveIssueCode::StudentNameEnglishTooLong, RosterSaveIssueSeverity::Error,
                   {{"maximumLength", std::int64_t{20}}});
        }
        if (Names::containsNonAscii(value))
            append(RosterSaveIssueCode::StudentNameEnglishNonAscii);
        if (Names::containsInvalidEnglishCharacters(value))
            append(RosterSaveIssueCode::StudentNameEnglishInvalidCharacters);
        return;
    }
    if (!validKoreanCharacters(value))
        append(RosterSaveIssueCode::StudentNameKoreanInvalidCharacters);
    const std::size_t length = koreanBaseNameLength(value);
    if (length == 0 || length == 3)
        return;
    const auto severity = allowQuestionableLengths
        ? RosterSaveIssueSeverity::Warning : RosterSaveIssueSeverity::Error;
    if (length <= 1)
        append(RosterSaveIssueCode::StudentNameKoreanTooShort, severity);
    else if (length >= 5)
        append(RosterSaveIssueCode::StudentNameKoreanTooLong, severity);
    else
        append(RosterSaveIssueCode::StudentNameKoreanUnusualLength,
               RosterSaveIssueSeverity::Warning);
}
} // namespace RosterSavePreparationDetail

// Prepares logical text (the UI's existing QString values). The comparator
// preserves Unicode case rules without Qt. Shapes and malformed values survive.
[[nodiscard]] inline RosterSavePreparation prepareRosterSaveText(
    const RosterSnapshot& snapshot,
    const bool allowQuestionableKoreanNameLengths,
    const RosterSaveCaseInsensitiveEquals& equals)
{
    using namespace RosterSavePreparationDetail;
    RosterSavePreparation result{.roster = snapshot};
    auto& roster = result.roster;
    for (auto& column : roster.columns)
    {
        column = normalizeRosterCustomColumnName(column, equals);
        for (const auto required : RosterSaveRequiredColumns)
        {
            if (equals(column, required))
            {
                column = required;
                break;
            }
        }
    }
    const int english = columnIndex(roster.columns, u"English", equals);
    const int korean = columnIndex(roster.columns, u"Korean", equals);
    for (auto& row : roster.rows)
    {
        for (std::size_t column = 0; column < row.size(); ++column)
        {
            const auto& value = row[column];
            row[column] = static_cast<int>(column) == english
                ? Names::normalizeEnglishName(value)
                : static_cast<int>(column) == korean
                    ? normalizeKoreanName(value)
                    : simplifyQtWhitespace(value);
        }
    }
    for (const auto required : RosterSaveRequiredColumns)
    {
        if (columnIndex(roster.columns, required, equals) < 0)
        {
            result.issues.push_back({.code = RosterSaveIssueCode::RosterColumnRequired,
                                    .field = u"columns",
                                    .arguments = {{"column", std::u16string(required)}}});
        }
    }
    for (std::size_t column = 0; column < roster.columns.size(); ++column)
    {
        const auto field = u"columns[" + number(column) + u"]";
        lengthIssue(result, roster.columns[column], 1, RosterSaveMaximumColumnNameLength,
                    field, -1, static_cast<int>(column));
        for (std::size_t previous = 0; previous < column; ++previous)
        {
            if (equals(roster.columns[column], roster.columns[previous]))
            {
                result.issues.push_back({.code = RosterSaveIssueCode::RosterColumnDuplicate,
                    .field = field,
                    .column = RosterSaveIssueColumnIndex{
                        static_cast<int>(column)
                        },
                    .arguments = {{"duplicateColumn", static_cast<std::int64_t>(previous)}}});
                break;
            }
        }
    }
    if (roster.columnWidths.size() > roster.columns.size())
    {
        result.issues.push_back({.code = RosterSaveIssueCode::RosterColumnWidthsInvalidCount,
            .field = u"columnWidths",
            .arguments = {{"widthCount", static_cast<std::int64_t>(roster.columnWidths.size())},
                          {"columnCount", static_cast<std::int64_t>(roster.columns.size())}}});
    }
    if (roster.rows.size() > RosterSaveMaximumRows)
    {
        result.issues.push_back({.code = RosterSaveIssueCode::RosterRowsTooMany, .field = u"rows",
            .arguments = {{"maximumRows", static_cast<std::int64_t>(RosterSaveMaximumRows)},
                          {"rowCount", static_cast<std::int64_t>(roster.rows.size())}}});
    }
    for (std::size_t rowIndex = 0; rowIndex < roster.rows.size(); ++rowIndex)
    {
        const auto& row = roster.rows[rowIndex];
        const int r = static_cast<int>(rowIndex);
        const auto rowField = u"rows[" + number(rowIndex) + u"]";
        if (row.size() > roster.columns.size())
        {
            result.issues.push_back({.code = RosterSaveIssueCode::RosterRowTooManyCells,
                .field = rowField,
                .row = RosterSaveIssueRowIndex{r},
                .arguments = {{"cellCount", static_cast<std::int64_t>(row.size())},
                              {"columnCount", static_cast<std::int64_t>(roster.columns.size())}}});
        }
        for (std::size_t column = 0; column < row.size(); ++column)
        {
            const auto field = column < roster.columns.size()
                ? rowField + u"." + roster.columns[column]
                : rowField + u".cells[" + number(column) + u"]";
            lengthIssue(result, row[column], 0, RosterSaveMaximumCellLength,
                        field, r, static_cast<int>(column));
        }
        if (!rosterRowHasData(row))
            continue;
        for (const auto& [column, name] : std::array<std::pair<int, std::u16string_view>, 2>{
                 {{english, u"English"}, {korean, u"Korean"}}})
        {
            const auto field = rowField + u"." + std::u16string(name);
            if (trimQtWhitespace(Names::cell(row, column)).empty())
            {
                result.issues.push_back({.code = RosterSaveIssueCode::RosterStudentNameRequired,
                    .field = field,
                    .row = RosterSaveIssueRowIndex{r},
                    .column = RosterSaveIssueColumnIndex{column},
                    .arguments = {{"field", field}}});
            }
        }
        if (!trimQtWhitespace(Names::cell(row, english)).empty())
            nameIssues(result, Names::cell(row, english), rowField + u".English", r, english, false, false);
        if (!trimQtWhitespace(Names::cell(row, korean)).empty())
            nameIssues(result, Names::cell(row, korean), rowField + u".Korean", r, korean, true,
                       allowQuestionableKoreanNameLengths);
    }
    if (english >= 0 && korean >= 0)
    {
        std::vector<std::optional<Domain::StudentNamePair>> pairs;
        for (const auto& row : roster.rows)
        {
            pairs.push_back(Domain::StudentNamePair::fromNames(
                trimQtWhitespace(Names::cell(row, english)), trimQtWhitespace(Names::cell(row, korean))));
        }
        for (const auto& duplicate : Domain::duplicateStudentNamePairGroups(pairs))
        {
            std::vector<Domain::DuplicateStudentNamePairRowIndex> rows;
            for (const auto row : duplicate.rowIndexes)
                rows.push_back(row);
            for (const auto rowIndex : rows)
            {
                const int row = static_cast<int>(rowIndex.rawValue());
                result.issues.push_back({.code = RosterSaveIssueCode::StudentNameDuplicatePair,
                    .field = u"English",
                    .row = RosterSaveIssueRowIndex{row},
                    .column = RosterSaveIssueColumnIndex{english},
                    .arguments = {{"duplicateRows", rows}}});
                result.issues.push_back({.code = RosterSaveIssueCode::StudentNameDuplicatePair,
                    .field = u"Korean",
                    .row = RosterSaveIssueRowIndex{row},
                    .column = RosterSaveIssueColumnIndex{korean},
                    .arguments = {{"duplicateRows", rows}}});
            }
        }
    }
    return result;
}

// Raw save snapshots preserve the old Platform fromUtf16 conversion boundary.
// The UI calls prepareRosterSaveText for feedback without changing its request.
[[nodiscard]] inline RosterSavePreparation prepareRosterSave(
    const RosterSnapshot& snapshot,
    const bool allowQuestionableKoreanNameLengths,
    const RosterSaveCaseInsensitiveEquals& equals)
{
    RosterSnapshot decoded = snapshot;
    for (auto& column : decoded.columns)
        column = RosterSavePreparationDetail::snapshotText(column);
    for (auto& row : decoded.rows)
        for (auto& cell : row)
            cell = RosterSavePreparationDetail::snapshotText(cell);
    return prepareRosterSaveText(decoded, allowQuestionableKoreanNameLengths, equals);
}

} // namespace ClassMngr::Next::Application

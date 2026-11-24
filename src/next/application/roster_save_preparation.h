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

[[nodiscard]] inline bool hasOnlyQuestionableKoreanNameLengthErrors(
    const std::vector<RosterSaveIssue>& issues
    )
{
    return ManualSaveEligibilityDetail::hasOnlyQuestionableKoreanNameLengthErrors(
        issues,
        [](const RosterSaveIssue& issue)
        {
            return issue.severity == RosterSaveIssueSeverity::Error;
        },
        [](const RosterSaveIssue& issue)
        {
            return issue.code == RosterSaveIssueCode::StudentNameKoreanTooShort
                || issue.code == RosterSaveIssueCode::StudentNameKoreanTooLong;
        }
        );
}

[[nodiscard]] inline bool hasOnlyQuestionableKoreanNameLengthErrors(
    const RosterSavePreparation& prepared
    )
{
    return hasOnlyQuestionableKoreanNameLengthErrors(prepared.issues);
}

inline constexpr std::size_t RosterSaveMaximumRows = RosterMaximumRowCount;
inline constexpr std::size_t RosterSaveMaximumColumnNameLength =
    RosterMaximumColumnNameLength;
inline constexpr std::size_t RosterSaveMaximumCellLength = 10000;
inline constexpr std::array<std::u16string_view, 6> RosterSaveRequiredColumns{
    u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"
};

// The reader copies each requested value into caller-owned scratch storage.
// Adapters can expose text from other string types without aliasing their
// storage or creating a full owning RosterSnapshot for issue-only validation.
struct RosterSaveTextReader final
{
    std::size_t columnCount = 0;
    std::size_t columnWidthCount = 0;
    std::size_t rowCount = 0;
    std::function<void(std::size_t, std::u16string&)> copyColumnText;
    std::function<std::size_t(std::size_t)> rowCellCount;
    std::function<void(std::size_t, std::size_t, std::u16string&)> copyCellText;
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

inline std::u16string normalizeColumnName(
    const std::u16string_view value,
    const RosterSaveCaseInsensitiveEquals& equals)
{
    std::u16string normalized = normalizeRosterCustomColumnName(value, equals);
    for (const auto required : RosterSaveRequiredColumns)
    {
        if (equals(normalized, required))
        {
            normalized = required;
            break;
        }
    }
    return normalized;
}

inline void simplifyCustomCellWhitespaceInto(
    const std::u16string_view value,
    std::u16string& destination)
{
    destination.clear();
    destination.reserve(value.size());

    bool hasContent = false;
    bool pendingSpace = false;
    for (const char16_t character : value)
    {
        if (isQtWhitespace(character))
        {
            pendingSpace = hasContent;
            continue;
        }

        if (pendingSpace)
            destination.push_back(u' ');
        destination.push_back(character);
        hasContent = true;
        pendingSpace = false;
    }
}

inline void normalizeCellText(
    const std::u16string_view value,
    const int column,
    const int english,
    const int korean,
    std::u16string& destination)
{
    if (column == english)
    {
        auto normalized = Names::normalizeEnglishName(value);
        if (normalized.size() > destination.capacity())
            destination = std::move(normalized);
        else
            destination.assign(normalized);
        return;
    }
    if (column == korean)
    {
        auto normalized = normalizeKoreanName(value);
        if (normalized.size() > destination.capacity())
            destination = std::move(normalized);
        else
            destination.assign(normalized);
        return;
    }
    simplifyCustomCellWhitespaceInto(value, destination);
}

inline bool containsNonWhitespace(const std::u16string_view value) noexcept
{
    return std::ranges::any_of(value, [](const char16_t character)
    {
        return !isQtWhitespace(character);
    });
}

inline void lengthIssue(std::vector<RosterSaveIssue>& issues,
                        const std::u16string_view value,
                        const std::size_t minimum, const std::size_t maximum,
                        const std::u16string& field, const int row, const int column)
{
    if (value.size() < minimum || value.size() > maximum)
    {
        issues.push_back({
            .code = RosterSaveIssueCode::ValidationLengthOutOfBounds, .field = field,
            .row = RosterSaveIssueRowIndex{row},
            .column = RosterSaveIssueColumnIndex{column},
            .arguments = {{"length", static_cast<std::int64_t>(value.size())},
                          {"minimum", static_cast<std::int64_t>(minimum)},
                          {"maximum", static_cast<std::int64_t>(maximum)}}
        });
    }
}

inline void nameIssues(std::vector<RosterSaveIssue>& issues,
                       const std::u16string_view value,
                       const std::u16string& field, const int row, const int column,
                       const bool korean, const bool allowQuestionableLengths)
{
    const auto append = [&](const RosterSaveIssueCode code,
                            const RosterSaveIssueSeverity severity = RosterSaveIssueSeverity::Error,
                            std::map<std::string, RosterSaveIssueArgument> arguments = {})
    {
        issues.push_back({
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

inline std::vector<RosterSaveIssue> collectIssues(
    const RosterSaveTextReader& source,
    const bool allowQuestionableKoreanNameLengths,
    const RosterSaveCaseInsensitiveEquals& equals,
    RosterSnapshot* const normalizedRoster)
{
    std::vector<RosterSaveIssue> issues;
    std::vector<std::u16string> columns;
    columns.reserve(source.columnCount);
    std::u16string scratch;
    for (std::size_t column = 0; column < source.columnCount; ++column)
    {
        source.copyColumnText(column, scratch);
        columns.push_back(normalizeColumnName(scratch, equals));
        if (normalizedRoster && column < normalizedRoster->columns.size())
            normalizedRoster->columns[column] = columns.back();
    }

    const int english = columnIndex(columns, u"English", equals);
    const int korean = columnIndex(columns, u"Korean", equals);
    for (const auto required : RosterSaveRequiredColumns)
    {
        if (columnIndex(columns, required, equals) < 0)
        {
            issues.push_back({.code = RosterSaveIssueCode::RosterColumnRequired,
                .field = u"columns",
                .arguments = {{"column", std::u16string(required)}}});
        }
    }
    for (std::size_t column = 0; column < columns.size(); ++column)
    {
        const auto field = u"columns[" + number(column) + u"]";
        lengthIssue(issues, columns[column], 1, RosterSaveMaximumColumnNameLength,
                    field, -1, static_cast<int>(column));
        for (std::size_t previous = 0; previous < column; ++previous)
        {
            if (equals(columns[column], columns[previous]))
            {
                issues.push_back({.code = RosterSaveIssueCode::RosterColumnDuplicate,
                    .field = field,
                    .column = RosterSaveIssueColumnIndex{static_cast<int>(column)},
                    .arguments = {{"duplicateColumn", static_cast<std::int64_t>(previous)}}});
                break;
            }
        }
    }
    if (source.columnWidthCount > source.columnCount)
    {
        issues.push_back({.code = RosterSaveIssueCode::RosterColumnWidthsInvalidCount,
            .field = u"columnWidths",
            .arguments = {{"widthCount", static_cast<std::int64_t>(source.columnWidthCount)},
                          {"columnCount", static_cast<std::int64_t>(source.columnCount)}}});
    }
    if (source.rowCount > RosterSaveMaximumRows)
    {
        issues.push_back({.code = RosterSaveIssueCode::RosterRowsTooMany, .field = u"rows",
            .arguments = {{"maximumRows", static_cast<std::int64_t>(RosterSaveMaximumRows)},
                          {"rowCount", static_cast<std::int64_t>(source.rowCount)}}});
    }

    std::vector<std::optional<Domain::StudentNamePair>> pairs;
    if (english >= 0 && korean >= 0)
        pairs.reserve(source.rowCount);
    std::u16string rawCell;
    std::u16string normalizedCell;
    for (std::size_t rowIndex = 0; rowIndex < source.rowCount; ++rowIndex)
    {
        const std::size_t cellCount = source.rowCellCount(rowIndex);
        const int row = static_cast<int>(rowIndex);
        const auto rowField = u"rows[" + number(rowIndex) + u"]";
        if (cellCount > source.columnCount)
        {
            issues.push_back({.code = RosterSaveIssueCode::RosterRowTooManyCells,
                .field = rowField,
                .row = RosterSaveIssueRowIndex{row},
                .arguments = {{"cellCount", static_cast<std::int64_t>(cellCount)},
                              {"columnCount", static_cast<std::int64_t>(source.columnCount)}}});
        }

        bool rowHasData = false;
        std::u16string englishText;
        std::u16string koreanText;
        for (std::size_t column = 0; column < cellCount; ++column)
        {
            source.copyCellText(rowIndex, column, rawCell);
            normalizeCellText(
                rawCell, static_cast<int>(column), english, korean, normalizedCell);
            if (normalizedCell.size() > RosterSaveMaximumCellLength)
            {
                const auto field = column < columns.size()
                    ? rowField + u"." + columns[column]
                    : rowField + u".cells[" + number(column) + u"]";
                lengthIssue(issues, normalizedCell, 0, RosterSaveMaximumCellLength,
                            field, row, static_cast<int>(column));
            }
            rowHasData = rowHasData || containsNonWhitespace(normalizedCell);
            if (static_cast<int>(column) == english)
                englishText = normalizedCell;
            if (static_cast<int>(column) == korean)
                koreanText = normalizedCell;
            if (normalizedRoster && rowIndex < normalizedRoster->rows.size()
                && column < normalizedRoster->rows[rowIndex].size())
            {
                normalizedRoster->rows[rowIndex][column] = normalizedCell;
            }
        }

        if (rowHasData)
        {
            for (const auto& [column, name] :
                 std::array<std::pair<int, std::u16string_view>, 2>{
                     {{english, u"English"}, {korean, u"Korean"}}})
            {
                const auto field = rowField + u"." + std::u16string(name);
                const std::u16string& value = column == english ? englishText : koreanText;
                if (!containsNonWhitespace(value))
                {
                    issues.push_back({.code = RosterSaveIssueCode::RosterStudentNameRequired,
                        .field = field,
                        .row = RosterSaveIssueRowIndex{row},
                        .column = RosterSaveIssueColumnIndex{column},
                        .arguments = {{"field", field}}});
                }
            }
            if (containsNonWhitespace(englishText))
                nameIssues(issues, englishText, rowField + u".English", row, english,
                           false, false);
            if (containsNonWhitespace(koreanText))
                nameIssues(issues, koreanText, rowField + u".Korean", row, korean,
                           true, allowQuestionableKoreanNameLengths);
        }
        if (english >= 0 && korean >= 0)
        {
            pairs.push_back(Domain::StudentNamePair::fromNames(
                trimQtWhitespace(englishText), trimQtWhitespace(koreanText)));
        }
    }

    if (english >= 0 && korean >= 0)
    {
        for (const auto& duplicate : Domain::duplicateStudentNamePairGroups(pairs))
        {
            std::vector<Domain::DuplicateStudentNamePairRowIndex> rows;
            for (const auto row : duplicate.rowIndexes)
                rows.push_back(row);
            for (const auto rowIndex : rows)
            {
                const int row = static_cast<int>(rowIndex.rawValue());
                issues.push_back({.code = RosterSaveIssueCode::StudentNameDuplicatePair,
                    .field = u"English",
                    .row = RosterSaveIssueRowIndex{row},
                    .column = RosterSaveIssueColumnIndex{english},
                    .arguments = {{"duplicateRows", rows}}});
                issues.push_back({.code = RosterSaveIssueCode::StudentNameDuplicatePair,
                    .field = u"Korean",
                    .row = RosterSaveIssueRowIndex{row},
                    .column = RosterSaveIssueColumnIndex{korean},
                    .arguments = {{"duplicateRows", rows}}});
            }
        }
    }
    return issues;
}
} // namespace RosterSavePreparationDetail

// Validates a borrowed text source without materializing its complete roster.
// Column and cell values are copied into reusable caller-owned scratch strings.
[[nodiscard]] inline std::vector<RosterSaveIssue> validateRosterSaveTextIssues(
    const RosterSaveTextReader& source,
    const bool allowQuestionableKoreanNameLengths,
    const RosterSaveCaseInsensitiveEquals& equals)
{
    return RosterSavePreparationDetail::collectIssues(
        source, allowQuestionableKoreanNameLengths, equals, nullptr);
}

// Prepares logical text (the UI's existing QString values). The comparator
// preserves Unicode case rules without Qt. Shapes and malformed values survive.
[[nodiscard]] inline RosterSavePreparation prepareRosterSaveText(
    RosterSnapshot snapshot,
    const bool allowQuestionableKoreanNameLengths,
    const RosterSaveCaseInsensitiveEquals& equals)
{
    using namespace RosterSavePreparationDetail;
    RosterSavePreparation result{.roster = std::move(snapshot)};
    auto& roster = result.roster;
    RosterSaveTextReader source{
        .columnCount = roster.columns.size(),
        .columnWidthCount = roster.columnWidths.size(),
        .rowCount = roster.rows.size(),
        .copyColumnText = [&roster](const std::size_t column, std::u16string& destination)
        {
            destination = roster.columns.at(column);
        },
        .rowCellCount = [&roster](const std::size_t row)
        {
            return roster.rows.at(row).size();
        },
        .copyCellText = [&roster](const std::size_t row, const std::size_t column,
                                  std::u16string& destination)
        {
            destination = roster.rows.at(row).at(column);
        }
    };
    result.issues = collectIssues(
        source, allowQuestionableKoreanNameLengths, equals, &roster);
    return result;
}

// Raw save snapshots preserve the old Platform fromUtf16 conversion boundary.
// The UI calls prepareRosterSaveText for feedback without changing its request.
[[nodiscard]] inline RosterSavePreparation prepareRosterSave(
    RosterSnapshot snapshot,
    const bool allowQuestionableKoreanNameLengths,
    const RosterSaveCaseInsensitiveEquals& equals)
{
    for (auto& column : snapshot.columns)
        column = RosterSavePreparationDetail::snapshotText(column);
    for (auto& row : snapshot.rows)
        for (auto& cell : row)
            cell = RosterSavePreparationDetail::snapshotText(cell);
    return prepareRosterSaveText(
        std::move(snapshot), allowQuestionableKoreanNameLengths, equals);
}

} // namespace ClassMngr::Next::Application

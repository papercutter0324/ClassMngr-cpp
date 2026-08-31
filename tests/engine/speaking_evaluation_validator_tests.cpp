#include "classmngr/engine/speaking_evaluation_validator.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace classmngr::engine;

namespace
{
bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationValidatorTests: "
              << message << '\n';
    return false;
}

bool hasIssue(
    const ValidationResult& result,
    std::string_view code,
    int row = -2,
    int column = -2,
    std::string_view field = {}
    )
{
    for (const ValidationIssue& issue : result.issues())
    {
        if (issue.code == code
            && (row == -2 || issue.row == row)
            && (column == -2 || issue.column == column)
            && (field.empty() || issue.field == field))
        {
            return true;
        }
    }

    return false;
}

bool hasIssueWithSeverity(
    const ValidationResult& result,
    std::string_view code,
    ValidationSeverity severity,
    int row,
    int column
    )
{
    for (const ValidationIssue& issue : result.issues())
    {
        if (issue.code == code
            && issue.severity == severity
            && issue.row == row
            && issue.column == column)
        {
            return true;
        }
    }

    return false;
}

int countIssues(
    const ValidationResult& result,
    std::string_view code
    )
{
    int count = 0;
    for (const ValidationIssue& issue : result.issues())
    {
        if (issue.code == code)
        {
            ++count;
        }
    }
    return count;
}

SpeakingEvaluationRow validRow()
{
    SpeakingEvaluationRow row(SpeakingEvaluationValidator::MaximumColumns);
    row[1] = "Alice";
    row[2] = "\xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98";
    return row;
}
} // namespace

int main()
{
    bool passed = true;

    const std::vector<std::pair<std::string, std::string>> scoreAliases{
        {" 1 ", "C"},
        {"2", "B"},
        {"3", "B+"},
        {"4", "A"},
        {"5", "A+"},
        {"\xe3\x85\x8a", "C"},
        {"\xe3\x85\xa0", "B"},
        {"\xe3\x85\xa0 +", "B+"},
        {"\xe3\x85\x81", "A"},
        {"\xe3\x85\x81+", "A+"},
        {" a + ", "A+"},
        {"b\t+", "B+"},
        {"unknown", "unknown"},
        {" A ? ", "A ?"}
    };
    for (const auto& [source, expected] : scoreAliases)
    {
        passed &= expect(
            SpeakingEvaluationValidator::normalizedScore(source) == expected,
            "score alias normalization changed"
            );
    }

    SpeakingEvaluationRow sourceRow(
        SpeakingEvaluationValidator::MaximumColumns + 1);
    sourceRow[0] = " 7 ";
    sourceRow[1] = "  aLiCe  ";
    sourceRow[2] = " \xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98 ";
    sourceRow[3] = " \xe3\x85\x81 ";
    sourceRow[4] = " 2 ";
    sourceRow[9] = "  comment  ";
    sourceRow[10] = " note\n";
    sourceRow[11] = "  extra cell  ";
    const SpeakingEvaluationRows sourceRows{
        sourceRow,
        {"  extra row index  "}
    };
    const SpeakingEvaluationRows normalized =
        SpeakingEvaluationValidator::normalized(sourceRows);
    passed &= expect(
        normalized.size() == 2
            && normalized[0].size()
                == SpeakingEvaluationValidator::MaximumColumns + 1
            && normalized[0][0] == "7"
            && normalized[0][1] == "Alice"
            && normalized[0][2]
                == "\xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98"
            && normalized[0][3] == "A"
            && normalized[0][4] == "B"
            && normalized[0][9] == "  comment  "
            && normalized[0][10] == " note\n"
            && normalized[0][11] == "  extra cell  "
            && normalized[1][0] == "extra row index",
        "row normalization did not preserve the grid shape"
        );
    const ValidationResult normalizedStructureResult =
        SpeakingEvaluationValidator::validate(
            1,
            " Winter ",
            normalized
            );
    passed &= expect(
        hasIssue(
            normalizedStructureResult,
            "speaking_evaluation.row.too_many_cells",
            0
            ),
        "extra normalized cells were not retained for validation"
        );
    passed &= expect(
        SpeakingEvaluationValidator::validate(
            1,
            " Winter ",
            {validRow()}
            ).isValid(),
        "normalized valid rows were rejected"
        );

    const std::string malformed(1, static_cast<char>(0xff));
    SpeakingEvaluationRows malformedRows{
        SpeakingEvaluationRow(SpeakingEvaluationValidator::MaximumColumns)
    };
    malformedRows[0][1] = malformed;
    malformedRows[0][2] = "\xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98";
    malformedRows[0][3] = " D ";
    const SpeakingEvaluationRows normalizedMalformed =
        SpeakingEvaluationValidator::normalized(malformedRows);
    passed &= expect(
        normalizedMalformed[0][1] == malformed
            && normalizedMalformed[0][3] == "D",
        "malformed values were silently converted into valid values"
        );

    SpeakingEvaluationRow invalidRow(
        SpeakingEvaluationValidator::MaximumColumns + 1);
    invalidRow[1] = "Alex1";
    invalidRow[2] = "\xea\xb9\x80!";
    invalidRow[3] = "D";
    invalidRow[9] = std::string(451, 'x');
    invalidRow[10] = std::string(10001, 'n');
    invalidRow[11] = "out-of-range cell";
    SpeakingEvaluationRows invalidRows(26);
    invalidRows[0] = invalidRow;
    const ValidationResult invalidResult =
        SpeakingEvaluationValidator::validate(
            0,
            "   ",
            invalidRows
            );
    passed &= expect(
        hasIssue(invalidResult, "speaking_evaluation.class_id.invalid")
            && hasIssue(
                invalidResult,
                "validation.length.out_of_bounds",
                -1,
                -1,
                "evaluationName"
                )
            && hasIssue(
                invalidResult,
                "speaking_evaluation.rows.too_many",
                -1,
                -1,
                "rows"
                )
            && hasIssue(
                invalidResult,
                "speaking_evaluation.row.too_many_cells",
                0,
                -1,
                "rows[0]"
                )
            && hasIssue(
                invalidResult,
                "student_name.english.invalid_characters",
                0,
                1,
                "rows[0].English Name"
                )
            && hasIssue(
                invalidResult,
                "student_name.korean.invalid_characters",
                0,
                2,
                "rows[0].Korean Name"
                )
            && hasIssue(
                invalidResult,
                "validation.enum.invalid_value",
                0,
                3,
                "rows[0].Grammar"
                )
            && hasIssue(
                invalidResult,
                "validation.length.out_of_bounds",
                0,
                9,
                "rows[0].Comments"
                )
            && hasIssue(
                invalidResult,
                "validation.length.out_of_bounds",
                0,
                10,
                "rows[0].Notes"
                ),
        "invalid name, score, length, or structure diagnostics changed"
        );

    SpeakingEvaluationRow missingNames(
        SpeakingEvaluationValidator::MaximumColumns);
    missingNames[3] = "A";
    const ValidationResult requiredResult =
        SpeakingEvaluationValidator::validate(
            1,
            "Winter",
            {missingNames}
            );
    passed &= expect(
        hasIssue(
            requiredResult,
            "speaking_evaluation.student_name.required",
            0,
            1,
            "rows[0].English Name"
            )
            && hasIssue(
                requiredResult,
                "speaking_evaluation.student_name.required",
                0,
                2,
                "rows[0].Korean Name"
                ),
        "required editable-row names were not reported"
        );

    const SpeakingEvaluationRow duplicateRow = validRow();
    const ValidationResult duplicateResult =
        SpeakingEvaluationValidator::validate(
            1,
            "Winter",
            {duplicateRow, duplicateRow}
            );
    passed &= expect(
        countIssues(duplicateResult, "student_name.duplicate_pair") == 4
            && hasIssue(
                duplicateResult,
                "student_name.duplicate_pair",
                0,
                1,
                "English Name"
                )
            && hasIssue(
                duplicateResult,
                "student_name.duplicate_pair",
                1,
                2,
                "Korean Name"
                ),
        "duplicate English/Korean name pairs were not reported"
        );

    SpeakingEvaluationRow questionableRow = validRow();
    questionableRow[2] = "\xea\xb9\x80";
    const ValidationResult questionableError =
        SpeakingEvaluationValidator::validate(
            1,
            "Winter",
            {questionableRow}
            );
    const ValidationResult questionableWarning =
        SpeakingEvaluationValidator::validate(
            1,
            "Winter",
            {questionableRow},
            true
            );
    passed &= expect(
        hasIssueWithSeverity(
            questionableError,
            "student_name.korean.too_short",
            ValidationSeverity::Error,
            0,
            2
            )
            && hasIssueWithSeverity(
                questionableWarning,
                "student_name.korean.too_short",
                ValidationSeverity::Warning,
                0,
                2
                )
            && questionableWarning.hasWarnings()
            && !questionableWarning.hasErrors(),
        "questionable Korean-name severity changed"
        );

    return passed ? 0 : 1;
}

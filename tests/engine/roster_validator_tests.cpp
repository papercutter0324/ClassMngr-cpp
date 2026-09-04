#include "classmngr/engine/roster_validator.h"
#include "classmngr/engine/student_name.h"

#include <algorithm>
#include <iostream>
#include <string_view>

using namespace classmngr::engine;

namespace
{
bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineRosterValidatorTests: "
              << message << '\n';
    return false;
}

bool hasIssue(
    const ValidationResult& result,
    std::string_view code,
    int row = -2,
    int column = -2
    )
{
    for (const ValidationIssue& issue : result.issues())
    {
        if (issue.code == code
            && (row == -2 || issue.row == row)
            && (column == -2 || issue.column == column))
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

bool hasNameIssue(
    const std::vector<StudentNameIssue>& issues,
    StudentNameIssue expected
    )
{
    return std::find(issues.cbegin(), issues.cend(), expected) != issues.cend();
}

} // namespace

int main()
{
    bool passed = true;

    Roster source;
    source.columns = {
        " english ",
        "KOREAN",
        "Winter",
        "Speech   Contest",
        "Summer",
        "Autumn",
        "Notes"
    };
    source.rows = {
        {
            "  aMY  ",
            " \xEA\xB9\x80 \xEB\xAF\xBC \xEC\x88\x98 (a) ",
            "  Present  ",
            "  Needs review  ",
            "  ",
            "  Ready ",
            "  Extra note  "
        }
    };

    const Roster normalized = RosterValidator::normalized(source);
    passed &= expect(
        normalized.columns
            == std::vector<std::string>{
                "English",
                "Korean",
                "Winter",
                "Speech Contest",
                "Summer",
                "Fall",
                "Notes"
            },
        "roster columns were not canonicalized"
        );
    passed &= expect(
        normalized.rows.front().at(0) == "Amy"
            && normalized.rows.front().at(1)
                == "\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98(A)"
            && normalized.rows.front().at(2) == "Present"
            && normalized.rows.front().at(6) == "Extra note",
        "roster cell normalization changed"
        );
    passed &= expect(
        RosterValidator::validate(normalized).isValid(),
        "normalized valid roster was rejected"
        );

    const auto validEnglish = StudentNameService::validateEnglish("Amy-Jane");
    const auto invalidEnglish = StudentNameService::validateEnglish("Amy1");
    const std::string invalidUtf8{"\xc3\x28", 2};
    const auto malformedEnglish = StudentNameService::validateEnglish(invalidUtf8);
    passed &= expect(
        validEnglish.empty()
            && hasNameIssue(
                invalidEnglish,
                StudentNameIssue::EnglishContainsInvalidCharacters
                )
            && hasNameIssue(
                malformedEnglish,
                StudentNameIssue::EnglishContainsNonAscii
                )
            && hasNameIssue(
                malformedEnglish,
                StudentNameIssue::EnglishContainsInvalidCharacters
                ),
        "shared English name rules did not cover invalid ASCII and UTF-8"
        );

    const auto validKorean = StudentNameService::validateKorean(
        "\xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98(A)"
        );
    const auto emptyKorean = StudentNameService::validateKorean("");
    const auto whitespaceKorean = StudentNameService::validateKorean(" \t\n ");
    const auto shortKorean = StudentNameService::validateKorean("\xea\xb9\x80");
    const auto unusualKorean = StudentNameService::validateKorean(
        "\xea\xb9\x80\xeb\xaf\xbc"
        );
    const auto longKorean = StudentNameService::validateKorean(
        "\xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98\xec\xa7\x80\xec\x9b\x90"
        );
    const auto invalidKorean = StudentNameService::validateKorean(
        "\xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98" "1"
        );
    passed &= expect(
        validKorean.empty()
            && emptyKorean.empty()
            && whitespaceKorean.empty()
            && hasNameIssue(shortKorean, StudentNameIssue::KoreanTooShort)
            && hasNameIssue(
                unusualKorean,
                StudentNameIssue::KoreanUnusualLength
                )
            && hasNameIssue(longKorean, StudentNameIssue::KoreanTooLong)
            && hasNameIssue(
                invalidKorean,
                StudentNameIssue::KoreanContainsInvalidCharacters
                )
            && StudentNameService::namePairKey(
                " Amy ",
                " \xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98(A) "
                )
                == "Amy\x1f\xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98(A)",
        "shared Korean name or duplicate-pair rules changed"
        );

    Roster invalid;
    invalid.columns = {"English", "Korean"};
    invalid.columnWidths = {1, 2, 3};
    invalid.rows = {{"Amy1", "\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98!", "extra"}};
    const ValidationResult invalidResult = RosterValidator::validate(invalid);
    passed &= expect(
        hasIssue(invalidResult, "roster.column.required")
            && hasIssue(invalidResult, "roster.column_widths.invalid_count")
            && hasIssue(invalidResult, "roster.row.too_many_cells", 0)
            && hasIssue(
                invalidResult,
                "student_name.english.invalid_characters",
                0,
                0
                )
            && hasIssue(
                invalidResult,
                "student_name.korean.invalid_characters",
                0,
                1
                ),
        "invalid roster diagnostics changed"
        );

    Roster duplicate;
    duplicate.columns = {
        "English",
        "Korean",
        "Winter",
        "Speech Contest",
        "Summer",
        "Fall"
    };
    duplicate.rows = {
        {"Amy", "\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98", "", "", "", ""},
        {"Amy", "\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98", "", "", "", ""}
    };
    const ValidationResult duplicateResult = RosterValidator::validate(duplicate);
    int duplicateIssueCount = 0;
    for (const ValidationIssue& issue : duplicateResult.issues())
    {
        if (issue.code == "student_name.duplicate_pair")
        {
            ++duplicateIssueCount;
        }
    }
    passed &= expect(
        duplicateIssueCount == 4,
        "duplicate student pairs were not reported for both cells"
        );

    Roster questionable;
    questionable.columns = {
        "English",
        "Korean",
        "Winter",
        "Speech Contest",
        "Summer",
        "Fall"
    };
    questionable.rows = {
        {"Amy", "\xEA\xB9\x80", "", "", "", ""}
    };
    const ValidationResult errorLength = RosterValidator::validate(questionable);
    const ValidationResult warningLength = RosterValidator::validate(
        questionable,
        true
        );
    passed &= expect(
        hasIssueWithSeverity(
            errorLength,
            "student_name.korean.too_short",
            ValidationSeverity::Error,
            0,
            1
            )
            && hasIssueWithSeverity(
                warningLength,
                "student_name.korean.too_short",
                ValidationSeverity::Warning,
                0,
                1
                ),
        "questionable Korean lengths did not change severity"
        );
    passed &= expect(
        warningLength.hasWarnings() && !warningLength.hasErrors(),
        "questionable Korean length did not become a warning"
        );

    Roster oversized;
    oversized.columns = {
        "English",
        "Korean",
        "Winter",
        "Speech Contest",
        "Summer",
        "Fall"
    };
    oversized.rows.resize(RosterValidator::MaximumRows + 1);
    const ValidationResult oversizedResult = RosterValidator::validate(oversized);
    passed &= expect(
        hasIssue(oversizedResult, "roster.rows.too_many"),
        "roster row limit changed"
        );

    return passed ? 0 : 1;
}

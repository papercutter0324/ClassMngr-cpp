#include "shared_validation.h"

#include "core/utils/student_name_utils.h"

namespace SharedValidation
{

ValidationResult englishName(
    const QString& value,
    ValidationLocation location
    )
{
    ValidationResult result;
    const auto issues = StudentNameUtils::validateEnglishName(value);

    if (issues.contains(StudentNameUtils::ValidationIssue::EnglishTooLong))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("student_name.english.too_long"),
            location,
            ValidationSeverity::Error,
            {{QStringLiteral("maximumLength"), 20}}
            ));
    }

    if (issues.contains(StudentNameUtils::ValidationIssue::EnglishContainsNonAscii))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("student_name.english.non_ascii"),
            location
            ));
    }

    if (issues.contains(
            StudentNameUtils::ValidationIssue::EnglishContainsInvalidCharacters
            ))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("student_name.english.invalid_characters"),
            location
            ));
    }

    return result;
}

ValidationResult koreanName(
    const QString& value,
    ValidationLocation location,
    bool allowQuestionableLength
    )
{
    ValidationResult result;
    const auto issues = StudentNameUtils::validateKoreanName(value);

    if (issues.contains(
            StudentNameUtils::ValidationIssue::KoreanContainsInvalidCharacters
            ))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("student_name.korean.invalid_characters"),
            location
            ));
    }

    if (issues.contains(StudentNameUtils::ValidationIssue::KoreanTooShort))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("student_name.korean.too_short"),
            location,
            allowQuestionableLength
                ? ValidationSeverity::Warning
                : ValidationSeverity::Error
            ));
    }
    else if (issues.contains(StudentNameUtils::ValidationIssue::KoreanTooLong))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("student_name.korean.too_long"),
            location,
            allowQuestionableLength
                ? ValidationSeverity::Warning
                : ValidationSeverity::Error
            ));
    }
    else if (issues.contains(
                 StudentNameUtils::ValidationIssue::KoreanUnusualLength
                 ))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("student_name.korean.unusual_length"),
            location,
            ValidationSeverity::Warning
            ));
    }

    return result;
}

} // namespace SharedValidation

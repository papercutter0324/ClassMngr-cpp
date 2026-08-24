#include "shared_validation.h"

#include "core/utils/colorutils.h"
#include "core/utils/file_name_utils.h"
#include "core/utils/student_name_utils.h"
#include "domain/rules/schedule_value_parser.h"

#include <QVariantList>

#include <utility>

namespace SharedValidation
{
namespace
{
ValidationResult singleIssue(
    const QString& code,
    ValidationLocation location,
    QVariantMap arguments = {},
    ValidationSeverity severity = ValidationSeverity::Error
    )
{
    return ValidationResult(ValidationRules::issue(
        code,
        std::move(location),
        severity,
        std::move(arguments)
        ));
}
}

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

ValidationResult duplicateNamePairs(
    const QList<QStringList>& rows,
    int englishColumn,
    int koreanColumn,
    QString englishField,
    QString koreanField
    )
{
    ValidationResult result;
    const auto duplicates = StudentNameUtils::duplicateRowsByNamePair(
        rows,
        englishColumn,
        koreanColumn
        );

    for (auto it = duplicates.cbegin(); it != duplicates.cend(); ++it)
    {
        QVariantList duplicateRows;
        duplicateRows.reserve(it.value().size());
        for (const int row : it.value())
        {
            duplicateRows.append(row);
        }

        for (const int row : it.value())
        {
            for (const ValidationLocation& location : {
                     ValidationLocation{
                         .field = englishField,
                         .row = row,
                         .column = englishColumn
                     },
                     ValidationLocation{
                         .field = koreanField,
                         .row = row,
                         .column = koreanColumn
                     }
                 })
            {
                result.add(ValidationRules::issue(
                    QStringLiteral("student_name.duplicate_pair"),
                    location,
                    ValidationSeverity::Error,
                    {{QStringLiteral("duplicateRows"), duplicateRows}}
                    ));
            }
        }
    }

    return result;
}

ValidationResult weekday(
    const QString& value,
    ValidationLocation location
    )
{
    if (ScheduleValueParser::parseWeekday(value))
    {
        return {};
    }

    return singleIssue(
        QStringLiteral("schedule.weekday.invalid"),
        std::move(location),
        {{QStringLiteral("value"), value}}
        );
}

ValidationResult time(
    const QString& value,
    ValidationLocation location
    )
{
    if (ScheduleValueParser::parseTime(value))
    {
        return {};
    }

    return singleIssue(
        QStringLiteral("schedule.time.invalid_format"),
        std::move(location),
        {{QStringLiteral("value"), value}}
        );
}

ValidationResult timeOrder(
    const QString& start,
    const QString& end,
    ValidationLocation startLocation,
    ValidationLocation endLocation
    )
{
    ValidationResult result = time(start, startLocation);
    result.merge(time(end, endLocation));

    const auto parsedStart = ScheduleValueParser::parseTime(start);
    const auto parsedEnd = ScheduleValueParser::parseTime(end);
    if (!parsedStart || !parsedEnd || parsedEnd->value > parsedStart->value)
    {
        return result;
    }

    result.add(ValidationRules::issue(
        QStringLiteral("schedule.time.end_not_after_start"),
        std::move(endLocation),
        ValidationSeverity::Error,
        {
            {QStringLiteral("start"), parsedStart->text},
            {QStringLiteral("end"), parsedEnd->text}
        }
        ));
    return result;
}

ValidationResult color(
    const QString& value,
    ValidationLocation location
    )
{
    const auto canonical = ColorUtils::canonicalHexColor(value);
    if (canonical)
    {
        return {};
    }

    return singleIssue(
        QStringLiteral("color.invalid_hex"),
        std::move(location),
        {{QStringLiteral("value"), value}}
        );
}

ValidationResult fileName(
    const QString& value,
    ValidationLocation location
    )
{
    const auto normalized = FileNameUtils::normalizedFilesystemSafeFileName(
        value,
        {}
        );
    if (normalized && *normalized == value)
    {
        return {};
    }

    return singleIssue(
        QStringLiteral("file_name.invalid"),
        std::move(location),
        {{QStringLiteral("value"), value}}
        );
}

} // namespace SharedValidation

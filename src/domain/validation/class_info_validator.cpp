#include "class_info_validator.h"

#include "core/utils/colorutils.h"
#include "domain/validation/class_time_validator.h"
#include "domain/validation/validation_rules.h"
#include "features/classes/config/class_info_config.h"

#include <QStringList>

namespace
{
constexpr qsizetype NotesMaximumLength = 10000;

ValidationLocation field(const QString& name)
{
    return {.field = name};
}

QString canonicalChoice(const QString& value, const QStringList& choices)
{
    const QString trimmed = value.trimmed();
    for (const QString& choice : choices)
    {
        if (choice.compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return choice;
        }
    }

    return trimmed;
}

ValidationResult allowedValue(
    const QString& value,
    const QStringList& allowedValues,
    const QString& name
    )
{
    if (allowedValues.contains(value))
    {
        return {};
    }

    return ValidationResult(ValidationRules::issue(
        QStringLiteral("class_info.value.not_allowed"),
        field(name),
        ValidationSeverity::Error,
        {{QStringLiteral("value"), value},
         {QStringLiteral("allowedValues"), allowedValues}}
        ));
}
}

ClassInfo ClassInfoValidator::normalized(const ClassInfo& info)
{
    ClassInfo normalized = info;
    normalized.classGrade = canonicalChoice(info.classGrade, ClassInfoConfig::Grades);
    const QStringList levels =
        ClassInfoConfig::levelsForGrade(normalized.classGrade);
    normalized.classLevel = canonicalChoice(info.classLevel, levels);
    if (ClassInfoConfig::Grades.contains(normalized.classGrade)
        && levels.contains(normalized.classLevel))
    {
        normalized.readingBook = canonicalChoice(
            info.readingBook,
            ClassInfoConfig::readingBooks(
                normalized.classGrade,
                normalized.classLevel
                )
            );
        normalized.essayBook = canonicalChoice(
            info.essayBook,
            ClassInfoConfig::essayBooks(
                normalized.classGrade,
                normalized.classLevel
                )
            );
    }
    else
    {
        normalized.readingBook = info.readingBook.trimmed();
        normalized.essayBook = info.essayBook.trimmed();
    }
    for (ClassTime& time : normalized.classTimes)
    {
        time = ClassTimeValidator::normalized(time);
    }
    for (ClassTime& time : normalized.intensiveTimes)
    {
        time = ClassTimeValidator::normalized(time);
    }
    normalized.notes = info.notes.trimmed();
    normalized.timeFillerActivities = info.timeFillerActivities.trimmed();

    if (const auto classColor = ColorUtils::canonicalHexColor(info.classColor))
    {
        normalized.classColor = *classColor;
    }
    else
    {
        normalized.classColor = info.classColor.trimmed();
    }

    if (const auto fontColor = ColorUtils::canonicalHexColor(info.fontColor))
    {
        normalized.fontColor = *fontColor;
    }
    else
    {
        normalized.fontColor = info.fontColor.trimmed();
    }

    return normalized;
}

ValidationResult ClassInfoValidator::validate(const ClassInfo& info)
{
    ValidationResult result;

    if (info.classId <= 0)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("class_info.class_id.invalid"),
            field(QStringLiteral("classId")),
            ValidationSeverity::Error,
            {{QStringLiteral("value"), info.classId}}
            ));
    }

    if (info.teacherId == 0 || info.teacherId < -1)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("class_info.teacher_id.invalid"),
            field(QStringLiteral("teacherId")),
            ValidationSeverity::Error,
            {{QStringLiteral("value"), info.teacherId}}
            ));
    }

    const QString grade = info.classGrade.trimmed();
    const QString level = info.classLevel.trimmed();
    if (grade.isEmpty() != level.isEmpty())
    {
        result.add(ValidationRules::issue(
            grade.isEmpty()
                ? QStringLiteral("class_info.grade.required")
                : QStringLiteral("class_info.level.required"),
            field(grade.isEmpty()
                ? QStringLiteral("classGrade")
                : QStringLiteral("classLevel"))
            ));
    }
    else if (!grade.isEmpty())
    {
        result.merge(allowedValue(
            grade,
            ClassInfoConfig::Grades,
            QStringLiteral("classGrade")
            ));

        const QStringList levels = ClassInfoConfig::levelsForGrade(grade);
        result.merge(allowedValue(
            level,
            levels,
            QStringLiteral("classLevel")
            ));

        if (ClassInfoConfig::Grades.contains(grade) && levels.contains(level))
        {
            result.merge(allowedValue(
                info.readingBook.trimmed(),
                ClassInfoConfig::readingBooks(grade, level),
                QStringLiteral("readingBook")
                ));
            result.merge(allowedValue(
                info.essayBook.trimmed(),
                ClassInfoConfig::essayBooks(grade, level),
                QStringLiteral("essayBook")
                ));
        }
    }
    else if (!info.readingBook.trimmed().isEmpty()
             || !info.essayBook.trimmed().isEmpty())
    {
        if (!info.readingBook.trimmed().isEmpty())
        {
            result.add(ValidationRules::issue(
                QStringLiteral("class_info.book.requires_grade_level"),
                field(QStringLiteral("readingBook"))
                ));
        }
        if (!info.essayBook.trimmed().isEmpty())
        {
            result.add(ValidationRules::issue(
                QStringLiteral("class_info.book.requires_grade_level"),
                field(QStringLiteral("essayBook"))
                ));
        }
    }

    if (!ColorUtils::canonicalHexColor(info.classColor))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("color.invalid_hex"),
            field(QStringLiteral("classColor")),
            ValidationSeverity::Error,
            {{QStringLiteral("value"), info.classColor}}
            ));
    }
    if (!ColorUtils::canonicalHexColor(info.fontColor))
    {
        result.add(ValidationRules::issue(
            QStringLiteral("color.invalid_hex"),
            field(QStringLiteral("fontColor")),
            ValidationSeverity::Error,
            {{QStringLiteral("value"), info.fontColor}}
            ));
    }

    result.merge(validateNotes(
        info.classId,
        info.notes,
        info.timeFillerActivities
        ));
    result.merge(ClassTimeValidator::validate(
        info.classTimes,
        QStringLiteral("classTimes")
        ));
    result.merge(ClassTimeValidator::validate(
        info.intensiveTimes,
        QStringLiteral("intensiveTimes")
        ));

    return result;
}

ValidationResult ClassInfoValidator::validateNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    ValidationResult result;
    if (classId <= 0)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("class_info.class_id.invalid"),
            field(QStringLiteral("classId")),
            ValidationSeverity::Error,
            {{QStringLiteral("value"), classId}}
            ));
    }

    result.merge(ValidationRules::textLength(
        notes,
        0,
        NotesMaximumLength,
        field(QStringLiteral("notes"))
        ));
    result.merge(ValidationRules::textLength(
        timeFillerActivities,
        0,
        NotesMaximumLength,
        field(QStringLiteral("timeFillerActivities"))
        ));
    return result;
}

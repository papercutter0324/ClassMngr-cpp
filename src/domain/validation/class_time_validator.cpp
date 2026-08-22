#include "class_time_validator.h"

#include "domain/rules/schedule_value_parser.h"
#include "domain/validation/shared_validation.h"
#include "domain/validation/validation_rules.h"

#include <QHash>
#include <QTime>
#include <QVariantList>

#include <optional>

namespace
{
ValidationLocation location(
    const QString& prefix,
    int row,
    int column,
    const QString& field
    )
{
    return {
        .field = QStringLiteral("%1[%2].%3").arg(prefix).arg(row).arg(field),
        .row = row,
        .column = column
    };
}

std::optional<QTime> parseClassTime(const QString& value)
{
    if (const auto canonical = ScheduleValueParser::parseTime(value))
    {
        return canonical->value;
    }

    const QString normalized = value.trimmed().toUpper();
    const QTime parsed = QTime::fromString(normalized, QStringLiteral("h:mm AP"));
    if (!parsed.isValid()
        || parsed.toString(QStringLiteral("h:mm AP")) != normalized)
    {
        return std::nullopt;
    }

    return parsed;
}

QString timeKey(const QTime& value)
{
    return value.toString(QStringLiteral("HH:mm"));
}
}

ClassTime ClassTimeValidator::normalized(const ClassTime& time)
{
    ClassTime normalized = time;

    if (const auto weekday = ScheduleValueParser::parseWeekday(time.day))
    {
        normalized.day = weekday->text;
    }
    else
    {
        normalized.day = time.day.trimmed();
    }

    if (const std::optional<QTime> start = parseClassTime(time.startTime))
    {
        normalized.startTime = start->toString(QStringLiteral("h:mm AP"));
    }
    else
    {
        normalized.startTime = time.startTime.trimmed();
    }

    if (const std::optional<QTime> end = parseClassTime(time.endTime))
    {
        normalized.endTime = end->toString(QStringLiteral("h:mm AP"));
    }
    else
    {
        normalized.endTime = time.endTime.trimmed();
    }

    return normalized;
}

ValidationResult ClassTimeValidator::validate(
    const QList<ClassTime>& times,
    const QString& fieldPrefix
    )
{
    ValidationResult result;
    QHash<QString, QList<int>> rowsBySlot;

    for (int row = 0; row < times.size(); ++row)
    {
        const ClassTime& time = times.at(row);
        const ValidationLocation dayLocation =
            location(fieldPrefix, row, 0, QStringLiteral("day"));
        const ValidationLocation startLocation =
            location(fieldPrefix, row, 1, QStringLiteral("startTime"));
        const ValidationLocation endLocation =
            location(fieldPrefix, row, 2, QStringLiteral("endTime"));

        result.merge(SharedValidation::weekday(time.day, dayLocation));

        const std::optional<QTime> start = parseClassTime(time.startTime);
        if (!start)
        {
            result.add(ValidationRules::issue(
                QStringLiteral("schedule.time.invalid_format"),
                startLocation,
                ValidationSeverity::Error,
                {{QStringLiteral("value"), time.startTime}}
                ));
        }

        const std::optional<QTime> end = parseClassTime(time.endTime);
        if (!end)
        {
            result.add(ValidationRules::issue(
                QStringLiteral("schedule.time.invalid_format"),
                endLocation,
                ValidationSeverity::Error,
                {{QStringLiteral("value"), time.endTime}}
                ));
        }

        if (start && end && *end <= *start)
        {
            result.add(ValidationRules::issue(
                QStringLiteral("schedule.time.end_not_after_start"),
                endLocation,
                ValidationSeverity::Error,
                {{QStringLiteral("start"), time.startTime},
                 {QStringLiteral("end"), time.endTime}}
                ));
        }

        const auto weekday = ScheduleValueParser::parseWeekday(time.day);
        if (weekday && start && end)
        {
            const QString slot = QStringLiteral("%1|%2|%3")
                .arg(weekday->text, timeKey(*start), timeKey(*end));
            rowsBySlot[slot].append(row);
        }
    }

    for (auto it = rowsBySlot.cbegin(); it != rowsBySlot.cend(); ++it)
    {
        if (it.value().size() < 2)
        {
            continue;
        }

        QVariantList duplicateRows;
        duplicateRows.reserve(it.value().size());
        for (const int row : it.value())
        {
            duplicateRows.append(row);
        }

        for (const int row : it.value())
        {
            result.add(ValidationRules::issue(
                QStringLiteral("class_time.duplicate_slot"),
                location(fieldPrefix, row, 1, QStringLiteral("startTime")),
                ValidationSeverity::Error,
                {{QStringLiteral("duplicateRows"), duplicateRows}}
                ));
        }
    }

    return result;
}

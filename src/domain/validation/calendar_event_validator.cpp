#include "calendar_event_validator.h"

#include "domain/validation/validation_rules.h"

#include <QDateTime>
#include <QHash>
#include <QVariantList>

#include <utility>

namespace
{
ValidationLocation field(const QString& name)
{
    return {.field = name};
}

QDate nextRepeatDate(
    const QDate& date,
    CalendarEventRepeatFrequency frequency
    )
{
    switch (frequency)
    {
    case CalendarEventRepeatFrequency::Daily:
        return date.addDays(1);

    case CalendarEventRepeatFrequency::Weekly:
        return date.addDays(7);

    case CalendarEventRepeatFrequency::Monthly:
        return date.addMonths(1);
    }

    return {};
}

void addIndexedIssues(
    ValidationResult& destination,
    const ValidationResult& source,
    int index
    )
{
    for (ValidationIssue issue : source.issues())
    {
        issue.field = issue.field.isEmpty()
            ? QStringLiteral("events[%1]").arg(index)
            : QStringLiteral("events[%1].%2").arg(index).arg(issue.field);
        issue.row = index;
        destination.add(std::move(issue));
    }
}
}

CalendarEvent CalendarEventValidator::normalized(const CalendarEvent& event)
{
    CalendarEvent normalized = event;
    normalized.title = event.title.simplified();
    normalized.eventType = event.eventType.trimmed();
    normalized.timeStatus = event.timeStatus.trimmed();
    normalized.repeatSeriesId = event.repeatSeriesId.trimmed();
    return normalized;
}

ValidationResult CalendarEventValidator::validate(const CalendarEvent& event)
{
    ValidationResult result;
    const QString title = event.title.simplified();

    if (title.isEmpty())
    {
        result.add(ValidationRules::issue(
            QStringLiteral("calendar.title.required"),
            field(QStringLiteral("title"))
            ));
    }
    else
    {
        result.merge(ValidationRules::textLength(
            title,
            1,
            MaximumTitleLength,
            field(QStringLiteral("title"))
            ));
    }
    result.merge(ValidationRules::stringEnumValue(
        event.eventType,
        calendarEventTypes(),
        field(QStringLiteral("eventType"))
        ));
    result.merge(ValidationRules::stringEnumValue(
        event.timeStatus,
        calendarEventTimeStatuses(),
        field(QStringLiteral("timeStatus"))
        ));
    result.merge(ValidationRules::textLength(
        event.repeatSeriesId,
        0,
        MaximumRepeatSeriesIdLength,
        field(QStringLiteral("repeatSeriesId"))
        ));

    if (!event.startDate.isValid())
    {
        result.add(ValidationRules::issue(
            QStringLiteral("calendar.date.invalid"),
            field(QStringLiteral("startDate"))
            ));
    }

    if (!event.endDate.isValid())
    {
        result.add(ValidationRules::issue(
            QStringLiteral("calendar.date.invalid"),
            field(QStringLiteral("endDate"))
            ));
    }

    if (event.startDate.isValid()
        && event.endDate.isValid()
        && event.endDate < event.startDate)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("calendar.date.end_before_start"),
            field(QStringLiteral("endDate")),
            ValidationSeverity::Error,
            {{QStringLiteral("startDate"), event.startDate},
             {QStringLiteral("endDate"), event.endDate}}
            ));
    }

    const bool knownTimeStatus = calendarEventTimeStatuses().contains(
        event.timeStatus
        );
    if (!knownTimeStatus)
    {
        return result;
    }

    if (event.allDay)
    {
        if (event.timeStatus != QStringLiteral("Timed"))
        {
            result.add(ValidationRules::issue(
                QStringLiteral("calendar.time_status.all_day_requires_timed"),
                field(QStringLiteral("timeStatus"))
                ));
        }

        return result;
    }

    if (event.timeStatus == QStringLiteral("Timed"))
    {
        if (!event.startTime.isValid())
        {
            result.add(ValidationRules::issue(
                QStringLiteral("calendar.time.invalid"),
                field(QStringLiteral("startTime"))
                ));
        }
        if (!event.endTime.isValid())
        {
            result.add(ValidationRules::issue(
                QStringLiteral("calendar.time.invalid"),
                field(QStringLiteral("endTime"))
                ));
        }

        if (event.startDate.isValid()
            && event.endDate.isValid()
            && event.startTime.isValid()
            && event.endTime.isValid()
            && QDateTime(event.endDate, event.endTime)
                <= QDateTime(event.startDate, event.startTime))
        {
            result.add(ValidationRules::issue(
                QStringLiteral("calendar.time.end_not_after_start"),
                field(QStringLiteral("endTime")),
                ValidationSeverity::Error,
                {{QStringLiteral("startDate"), event.startDate},
                 {QStringLiteral("startTime"), event.startTime},
                 {QStringLiteral("endDate"), event.endDate},
                 {QStringLiteral("endTime"), event.endTime}}
                ));
        }

        return result;
    }

    if (event.startTime.isValid() || event.endTime.isValid())
    {
        result.add(ValidationRules::issue(
            QStringLiteral("calendar.time_status.requires_empty_times"),
            field(QStringLiteral("timeStatus")),
            ValidationSeverity::Error,
            {{QStringLiteral("timeStatus"), event.timeStatus}}
            ));
    }

    return result;
}

ValidationResult CalendarEventValidator::validateRecurrence(
    const CalendarEvent& event,
    CalendarEventRepeatFrequency frequency,
    const QDate& untilDate
    )
{
    ValidationResult result = validate(event);
    result.merge(ValidationRules::enumValue(
        frequency,
        {CalendarEventRepeatFrequency::Daily,
         CalendarEventRepeatFrequency::Weekly,
         CalendarEventRepeatFrequency::Monthly},
        field(QStringLiteral("repeat.frequency"))
        ));

    if (!untilDate.isValid())
    {
        result.add(ValidationRules::issue(
            QStringLiteral("calendar.repeat.until_date.invalid"),
            field(QStringLiteral("repeat.untilDate"))
            ));
        return result;
    }

    if (!event.startDate.isValid())
    {
        return result;
    }

    if (untilDate < event.startDate)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("calendar.repeat.until_before_start"),
            field(QStringLiteral("repeat.untilDate")),
            ValidationSeverity::Error,
            {{QStringLiteral("startDate"), event.startDate},
             {QStringLiteral("untilDate"), untilDate}}
            ));
        return result;
    }

    const int occurrenceCount = estimatedRepeatOccurrences(
        event.startDate,
        untilDate,
        frequency
        );
    if (occurrenceCount > MaximumRepeatOccurrences)
    {
        result.add(ValidationRules::issue(
            QStringLiteral("calendar.repeat.too_many_occurrences"),
            field(QStringLiteral("repeat.untilDate")),
            ValidationSeverity::Error,
            {{QStringLiteral("count"), occurrenceCount},
             {QStringLiteral("maximum"), MaximumRepeatOccurrences}}
            ));
    }

    return result;
}

ValidationResult CalendarEventValidator::validateSeries(
    const QList<CalendarEvent>& events
    )
{
    ValidationResult result;
    QHash<QString, QList<int>> rowsByRepeatSeries;

    for (int index = 0; index < events.size(); ++index)
    {
        const CalendarEvent& event = events.at(index);
        addIndexedIssues(result, validate(event), index);

        if (!event.repeatSeriesId.isEmpty())
        {
            rowsByRepeatSeries[event.repeatSeriesId].append(index);
        }
    }

    for (auto it = rowsByRepeatSeries.cbegin(); it != rowsByRepeatSeries.cend(); ++it)
    {
        if (it.value().size() <= MaximumRepeatOccurrences)
        {
            continue;
        }

        QVariantList rows;
        rows.reserve(it.value().size());
        for (const int row : it.value())
        {
            rows.append(row);
        }

        for (const int row : it.value())
        {
            result.add(ValidationRules::issue(
                QStringLiteral("calendar.repeat.too_many_occurrences"),
                {.field = QStringLiteral("events[%1].repeatSeriesId").arg(row),
                 .row = row},
                ValidationSeverity::Error,
                {{QStringLiteral("seriesId"), it.key()},
                 {QStringLiteral("rows"), rows},
                 {QStringLiteral("maximum"), MaximumRepeatOccurrences}}
                ));
        }
    }

    return result;
}

int CalendarEventValidator::estimatedRepeatOccurrences(
    const QDate& startDate,
    const QDate& untilDate,
    CalendarEventRepeatFrequency frequency
    )
{
    if (!startDate.isValid() || !untilDate.isValid() || untilDate < startDate)
    {
        return 0;
    }

    int occurrences = 0;
    for (QDate occurrence = startDate;
         occurrence.isValid() && occurrence <= untilDate;
         occurrence = nextRepeatDate(occurrence, frequency))
    {
        ++occurrences;
        if (occurrences > MaximumRepeatOccurrences)
        {
            return occurrences;
        }
    }

    return occurrences;
}

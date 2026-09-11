#include "calendar_event_validator.h"

#include "classmngr/engine/calendar_event_validator.h"

#include <vector>

namespace
{
void restoreLengthArguments(
    ValidationIssue& issue,
    const QString& value,
    qsizetype minimum,
    qsizetype maximum
    )
{
    issue.arguments = {
        {QStringLiteral("length"), static_cast<qlonglong>(value.size())},
        {QStringLiteral("minimum"), static_cast<qlonglong>(minimum)},
        {QStringLiteral("maximum"), static_cast<qlonglong>(maximum)}
    };
}

void restoreEventArguments(
    ValidationIssue& issue,
    const CalendarEvent& event
    )
{
    if (issue.code != QStringLiteral("validation.length.out_of_bounds"))
    {
        return;
    }

    if (issue.field == QStringLiteral("title"))
    {
        restoreLengthArguments(
            issue,
            event.title.simplified(),
            1,
            CalendarEventValidator::MaximumTitleLength
            );
    }
    else if (issue.field == QStringLiteral("repeatSeriesId"))
    {
        restoreLengthArguments(
            issue,
            event.repeatSeriesId,
            0,
            CalendarEventValidator::MaximumRepeatSeriesIdLength
            );
    }
}

ValidationResult fromEngine(
    const classmngr::engine::ValidationResult& validation,
    const CalendarEvent* event = nullptr,
    const QList<CalendarEvent>* events = nullptr
    )
{
    ValidationResult result;
    for (const classmngr::engine::ValidationIssue& source :
         validation.issues())
    {
        ValidationIssue issue{
            .code = calendar_event_detail::fromUtf8(source.code),
            .field = calendar_event_detail::fromUtf8(source.field),
            .row = source.row,
            .column = source.column,
            .severity = source.isWarning()
                ? ValidationSeverity::Warning
                : ValidationSeverity::Error
        };

        if (event)
        {
            restoreEventArguments(issue, *event);
        }
        else if (events && issue.code ==
                     QStringLiteral("validation.length.out_of_bounds"))
        {
            for (int index = 0; index < events->size(); ++index)
            {
                const QString prefix =
                    QStringLiteral("events[%1].").arg(index);
                if (issue.field == prefix + QStringLiteral("title"))
                {
                    restoreLengthArguments(
                        issue,
                        events->at(index).title.simplified(),
                        1,
                        CalendarEventValidator::MaximumTitleLength
                        );
                    break;
                }
                if (issue.field == prefix + QStringLiteral("repeatSeriesId"))
                {
                    restoreLengthArguments(
                        issue,
                        events->at(index).repeatSeriesId,
                        0,
                        CalendarEventValidator::MaximumRepeatSeriesIdLength
                        );
                    break;
                }
            }
        }

        result.add(std::move(issue));
    }
    return result;
}
} // namespace

CalendarEvent CalendarEventValidator::normalized(const CalendarEvent& event)
{
    return calendarEventFromEngine(
        classmngr::engine::CalendarEventValidator::normalized(
            calendarEventToEngine(event)
            )
        );
}

ValidationResult CalendarEventValidator::validate(const CalendarEvent& event)
{
    return fromEngine(
        classmngr::engine::CalendarEventValidator::validate(
            calendarEventToEngine(event)
            ),
        &event
        );
}

ValidationResult CalendarEventValidator::validateRecurrence(
    const CalendarEvent& event,
    CalendarEventRepeatFrequency frequency,
    const QDate& untilDate
    )
{
    return fromEngine(
        classmngr::engine::CalendarEventValidator::validateRecurrence(
            calendarEventToEngine(event),
            frequency,
            calendar_event_detail::toEngineDate(untilDate)
            ),
        &event
        );
}

ValidationResult CalendarEventValidator::validateSeries(
    const QList<CalendarEvent>& events
    )
{
    std::vector<classmngr::engine::CalendarEvent> engineEvents;
    engineEvents.reserve(static_cast<std::size_t>(events.size()));
    for (const CalendarEvent& event : events)
    {
        engineEvents.push_back(calendarEventToEngine(event));
    }

    return fromEngine(
        classmngr::engine::CalendarEventValidator::validateSeries(
            engineEvents
            ),
        nullptr,
        &events
        );
}

int CalendarEventValidator::estimatedRepeatOccurrences(
    const QDate& startDate,
    const QDate& untilDate,
    CalendarEventRepeatFrequency frequency
    )
{
    return classmngr::engine::CalendarEventValidator::estimatedRepeatOccurrences(
        calendar_event_detail::toEngineDate(startDate),
        calendar_event_detail::toEngineDate(untilDate),
        frequency
        );
}

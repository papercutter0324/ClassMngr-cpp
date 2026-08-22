#pragma once

#include "domain/models/calendar_event.h"
#include "domain/validation/validation_result.h"

#include <QList>

class CalendarEventValidator final
{
public:
    static constexpr int MaximumTitleLength = 255;
    static constexpr int MaximumRepeatSeriesIdLength = 128;
    static constexpr int MaximumRepeatOccurrences = 366;

    [[nodiscard]] static CalendarEvent normalized(const CalendarEvent& event);
    [[nodiscard]] static ValidationResult validate(const CalendarEvent& event);
    [[nodiscard]] static ValidationResult validateRecurrence(
        const CalendarEvent& event,
        CalendarEventRepeatFrequency frequency,
        const QDate& untilDate
        );
    [[nodiscard]] static ValidationResult validateSeries(
        const QList<CalendarEvent>& events
        );
    [[nodiscard]] static int estimatedRepeatOccurrences(
        const QDate& startDate,
        const QDate& untilDate,
        CalendarEventRepeatFrequency frequency
        );
};

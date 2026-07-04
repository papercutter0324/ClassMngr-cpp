#pragma once

#include "domain/models/calendar_event.h"

#include <QList>

namespace CalendarImport
{
struct Workbook;

struct ParsedCalendarImport
{
    QList<CalendarEvent> events;
    int skippedCount = 0;
};

ParsedCalendarImport parseCalendarEventsFromWorkbook(
    const Workbook& workbook
    );

QString calendarEventImportSignature(
    const CalendarEvent& event
    );
}

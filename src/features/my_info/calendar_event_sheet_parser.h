#pragma once

#include "domain/models/calendar_event.h"

#include <QList>
#include <QStringList>

namespace CalendarImport
{
struct Workbook;

struct ParsedCalendarImport
{
    QList<CalendarEvent> events;
    int skippedCount = 0;
};

ParsedCalendarImport parseCalendarEventsFromWorkbook(
    const Workbook& workbook,
    const QStringList& campusCodes = {}
    );

QString calendarEventImportSignature(
    const CalendarEvent& event
    );
}

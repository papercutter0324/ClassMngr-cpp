#pragma once

#include "domain/models/calendar_event.h"
#include "next/application/calendar_event_import_signature.h"

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

ClassMngr::Next::Application::CalendarEventImportSignature
calendarEventImportSignature(
    const CalendarEvent& event
    );
}

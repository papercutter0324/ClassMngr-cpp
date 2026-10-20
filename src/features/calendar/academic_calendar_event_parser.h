#pragma once

#include "domain/models/calendar_event.h"
#include "next/application/calendar_event_import_signature.h"
#include "next/application/calendar_event_import_count.h"

#include <QList>
#include <QStringList>

namespace CalendarImport
{
struct Workbook;
struct WorkbookMetadata;
struct Worksheet;

struct ParsedCalendarImport
{
    QList<CalendarEvent> events;
    ClassMngr::Next::Application::CalendarEventImportCount skippedCount;
};

ParsedCalendarImport parseCalendarEventsFromWorkbook(
    const Workbook& workbook,
    const QStringList& campusCodes = {}
    );

ParsedCalendarImport parseCalendarEventsFromWorksheet(
    const WorkbookMetadata& metadata,
    const Worksheet& worksheet,
    const QStringList& campusCodes = {}
    );

ClassMngr::Next::Application::CalendarEventImportSignature
calendarEventImportSignature(
    const CalendarEvent& event
    );
}

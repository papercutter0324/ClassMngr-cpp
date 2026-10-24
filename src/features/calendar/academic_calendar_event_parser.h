#pragma once

#include "domain/models/calendar_event.h"
#include "next/application/calendar_event_import_signature.h"
#include "next/application/calendar_event_import_count.h"

#include <QByteArray>
#include <QIODevice>
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

struct CalendarWorkbookReadCounts
{
    int worksheetCount = 0;
    int cellCount = 0;
    int mergedRangeCount = 0;
    int sharedStringCount = 0;
    int styleCount = 0;
};

ParsedCalendarImport parseCalendarEventsFromWorkbook(
    const Workbook& workbook,
    const QStringList& campusCodes = {}
    );

ParsedCalendarImport parseCalendarEventsFromWorkbookBytes(
    const QByteArray& data,
    const QStringList& campusCodes,
    QString* errorMessage,
    CalendarWorkbookReadCounts* readCounts = nullptr
    );

ParsedCalendarImport parseCalendarEventsFromWorkbookDevice(
    QIODevice& device,
    const QStringList& campusCodes,
    QString* errorMessage,
    CalendarWorkbookReadCounts* readCounts = nullptr
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

#include "features/calendar/academic_calendar_event_parser.h"
#include "features/calendar/calendar_workbook_reader.h"
#include "features/schedule/import/schedule_workbook_parser.h"
#include "features/teacher/import/teacher_import_file_validator.h"
#include "features/teacher/import/teacher_import_template_registry.h"

#include "core/zip_archive_read_adapter.h"

namespace CalendarImport
{
bool forEachWorksheet(
    const QByteArray& data,
    const WorksheetConsumer& consumer,
    QString* errorMessage
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return forEachWorksheet(data, *archiveReader, consumer, errorMessage);
}

bool forEachWorksheet(
    QIODevice& device,
    const WorksheetConsumer& consumer,
    QString* errorMessage
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return forEachWorksheet(device, *archiveReader, consumer, errorMessage);
}

bool forEachWorksheetRows(
    const QByteArray& data,
    const WorksheetStreamConsumer& consumer,
    QString* errorMessage
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return forEachWorksheetRows(data, *archiveReader, consumer, errorMessage);
}

bool forEachWorksheetRows(
    QIODevice& device,
    const WorksheetStreamConsumer& consumer,
    QString* errorMessage
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return forEachWorksheetRows(device, *archiveReader, consumer, errorMessage);
}

Workbook parseWorkbook(
    const QByteArray& data,
    QString* errorMessage
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return parseWorkbook(data, *archiveReader, errorMessage);
}

Workbook parseWorkbook(
    QIODevice& device,
    QString* errorMessage
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return parseWorkbook(device, *archiveReader, errorMessage);
}

ParsedCalendarImport parseCalendarEventsFromWorkbookBytes(
    const QByteArray& data,
    const QStringList& campusCodes,
    QString* errorMessage,
    CalendarWorkbookReadCounts* readCounts
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return parseCalendarEventsFromWorkbookBytes(
        data,
        *archiveReader,
        campusCodes,
        errorMessage,
        readCounts
        );
}

ParsedCalendarImport parseCalendarEventsFromWorkbookDevice(
    QIODevice& device,
    const QStringList& campusCodes,
    QString* errorMessage,
    CalendarWorkbookReadCounts* readCounts
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return parseCalendarEventsFromWorkbookDevice(
        device,
        *archiveReader,
        campusCodes,
        errorMessage,
        readCounts
        );
}
}

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    const QByteArray& data,
    ScheduleImportKind kind
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return parseScheduleImportWorkbook(data, *archiveReader, kind);
}

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    QIODevice& device,
    ScheduleImportKind kind
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return parseScheduleImportWorkbook(device, *archiveReader, kind);
}

TeacherImportFileValidation validateTeacherImportData(
    const QByteArray& data,
    const TeacherImportTemplateRegistry& registry
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return validateTeacherImportData(data, *archiveReader, registry);
}

TeacherImportFileValidation validateTeacherImportFile(
    const QString& filePath
    )
{
    const std::unique_ptr<ArchiveReadPort> archiveReader =
        makeZipArchiveReadPort();
    return validateTeacherImportFile(filePath, *archiveReader);
}

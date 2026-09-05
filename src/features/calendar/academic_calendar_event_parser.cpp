#include "academic_calendar_event_parser.h"

#include "calendar_workbook_reader.h"
#include "core/platform/qt_platform_services.h"
#include "classmngr/engine/calendar_event_import_service.h"

#include <QByteArray>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace CalendarImport
{
namespace
{
std::string utf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

classmngr::engine::CalendarImportWorkbook toEngineWorkbook(
    const Workbook& workbook
    )
{
    classmngr::engine::CalendarImportWorkbook result;
    result.cells.reserve(static_cast<std::size_t>(workbook.cells.size()));
    result.styles.reserve(static_cast<std::size_t>(workbook.styles.size()));

    for (const Cell& cell : workbook.cells)
    {
        result.cells.push_back({
            cell.row,
            cell.column,
            cell.style,
            utf8(cell.value),
            utf8(cell.note)
        });
    }

    for (const Style& style : workbook.styles)
    {
        result.styles.push_back({
            utf8(style.fillColor),
            utf8(style.fontColor)
        });
    }

    return result;
}

std::vector<std::string> toEngineCampusCodes(
    const QStringList& campusCodes
    )
{
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(campusCodes.size()));

    for (const QString& code : campusCodes)
    {
        result.push_back(utf8(code));
    }

    return result;
}
} // namespace

QString calendarEventImportSignature(
    const CalendarEvent& event
    )
{
    return fromUtf8(
        classmngr::engine::CalendarEventImportService::importSignature(
            calendarEventToEngine(event)
            )
        );
}

ParsedCalendarImport parseCalendarEventsFromWorkbook(
    const Workbook& workbook,
    const QStringList& campusCodes
    )
{
    const classmngr::qt::QtClock clock;
    const classmngr::engine::CalendarImportResult parsed =
        classmngr::engine::CalendarEventImportService::parse(
            toEngineWorkbook(workbook),
            toEngineCampusCodes(campusCodes),
            clock
            );

    ParsedCalendarImport result;
    result.skippedCount = parsed.skippedCount;
    result.events.reserve(static_cast<qsizetype>(parsed.events.size()));

    for (const classmngr::engine::CalendarEvent& event : parsed.events)
    {
        result.events.append(calendarEventFromEngine(event));
    }

    return result;
}

} // namespace CalendarImport

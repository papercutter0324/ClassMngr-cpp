#include "schedule_workbook_parser.h"

#include "classmngr/engine/schedule_workbook_interpreter.h"
#include "features/calendar/calendar_workbook_reader.h"

#include <QObject>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using EngineWorkbook = classmngr::engine::ScheduleImportWorkbook;
using EngineSheet = classmngr::engine::ScheduleImportSheet;
using EngineUser = classmngr::engine::ScheduleImportUserBlock;
using EngineCandidate = classmngr::engine::ScheduleImportClassCandidate;
using EngineDiagnostic = classmngr::engine::ScheduleImportDiagnostic;
using EngineTime = classmngr::engine::ClassTime;
using EngineSlotState = classmngr::engine::IntensiveSlotState;

std::string toUtf8(const QString& value)
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

classmngr::engine::ScheduleImportKind toEngineKind(ScheduleImportKind kind)
{
    return kind == ScheduleImportKind::Intensive
        ? classmngr::engine::ScheduleImportKind::Intensive
        : classmngr::engine::ScheduleImportKind::Normal;
}

classmngr::engine::ScheduleWorkbookLayout toLayout(
    const CalendarImport::Workbook& workbook
    )
{
    classmngr::engine::ScheduleWorkbookLayout layout;
    layout.styles.reserve(static_cast<std::size_t>(workbook.styles.size()));
    for (const CalendarImport::Style& style : workbook.styles)
    {
        layout.styles.push_back({
            toUtf8(style.fillColor),
            toUtf8(style.fontColor),
            style.filled,
            style.bold
        });
    }

    layout.sheets.reserve(
        static_cast<std::size_t>(workbook.worksheets.size())
        );
    for (const CalendarImport::Worksheet& worksheet : workbook.worksheets)
    {
        classmngr::engine::ScheduleWorkbookLayoutSheet sheet;
        sheet.name = toUtf8(worksheet.name);
        sheet.visible = worksheet.visible;
        sheet.cells.reserve(
            static_cast<std::size_t>(worksheet.cells.size())
            );
        for (const CalendarImport::Cell& cell : worksheet.cells)
        {
            sheet.cells.push_back({
                cell.row,
                cell.column,
                cell.style,
                toUtf8(cell.value),
                toUtf8(cell.note)
            });
        }
        sheet.mergedRanges.reserve(
            static_cast<std::size_t>(worksheet.mergedRanges.size())
            );
        for (const CalendarImport::CellRange& range : worksheet.mergedRanges)
        {
            sheet.mergedRanges.push_back({
                range.firstRow,
                range.firstColumn,
                range.lastRow,
                range.lastColumn
            });
        }
        layout.sheets.push_back(std::move(sheet));
    }
    return layout;
}

QList<ClassTime> fromEngineTimes(const std::vector<EngineTime>& source)
{
    QList<ClassTime> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const EngineTime& time : source)
    {
        result.append({
            fromUtf8(time.day),
            fromUtf8(time.startTime),
            fromUtf8(time.endTime)
        });
    }
    return result;
}

QList<IntensiveSlotState> fromEngineSlotStates(
    const std::vector<EngineSlotState>& source
    )
{
    QList<IntensiveSlotState> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const EngineSlotState& state : source)
    {
        result.append({
            fromUtf8(state.day),
            fromUtf8(state.startTime),
            fromUtf8(state.state)
        });
    }
    return result;
}

QList<ScheduleImportDiagnostic> fromEngineDiagnostics(
    const std::vector<EngineDiagnostic>& source
    )
{
    QList<ScheduleImportDiagnostic> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const EngineDiagnostic& diagnostic : source)
    {
        result.append({
            fromUtf8(diagnostic.sheetName),
            fromUtf8(diagnostic.userName),
            fromUtf8(diagnostic.cellReference),
            fromUtf8(diagnostic.value),
            fromUtf8(diagnostic.message)
        });
    }
    return result;
}

ScheduleImportClassCandidate fromEngineCandidate(
    const EngineCandidate& source
    )
{
    ScheduleImportClassCandidate result;
    result.teacherKey = fromUtf8(source.teacherKey);
    result.teacherKr = fromUtf8(source.teacherKr);
    for (const std::string& room : source.rooms)
    {
        result.rooms.append(fromUtf8(room));
    }
    for (const std::string& color : source.importedColors)
    {
        result.importedColors.append(fromUtf8(color));
    }
    result.classGrade = fromUtf8(source.classGrade);
    result.classLevel = fromUtf8(source.classLevel);
    result.times = fromEngineTimes(source.times);
    for (const std::string& cell : source.sourceCells)
    {
        result.sourceCells.append(fromUtf8(cell));
    }
    result.meetingPatternError = fromUtf8(source.meetingPatternError);
    return result;
}

ScheduleImportUserBlock fromEngineUser(const EngineUser& source)
{
    ScheduleImportUserBlock result;
    result.name = fromUtf8(source.name);
    result.headerCell = fromUtf8(source.headerCell);
    result.classes.reserve(static_cast<qsizetype>(source.classes.size()));
    for (const EngineCandidate& candidate : source.classes)
    {
        result.classes.append(fromEngineCandidate(candidate));
    }
    result.intensiveSlotStates = fromEngineSlotStates(source.intensiveSlotStates);
    result.diagnostics = fromEngineDiagnostics(source.diagnostics);
    return result;
}

ScheduleImportWorkbook fromEngineWorkbook(const EngineWorkbook& source)
{
    ScheduleImportWorkbook result;
    result.sheets.reserve(static_cast<qsizetype>(source.sheets.size()));
    for (const EngineSheet& sheet : source.sheets)
    {
        ScheduleImportSheet converted;
        converted.name = fromUtf8(sheet.name);
        converted.visible = sheet.visible;
        converted.users.reserve(static_cast<qsizetype>(sheet.users.size()));
        for (const EngineUser& user : sheet.users)
        {
            converted.users.append(fromEngineUser(user));
        }
        converted.diagnostics = fromEngineDiagnostics(sheet.diagnostics);
        result.sheets.append(std::move(converted));
    }
    return result;
}
} // namespace

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    const QByteArray& data,
    ScheduleImportKind kind,
    const ScheduleImportCancellation& isCancelled
    )
{
    const auto cancelled = [&isCancelled] {
        return isCancelled && isCancelled();
    };
    if (cancelled())
    {
        return std::unexpected(QObject::tr(
            "The schedule import was cancelled."
            ));
    }

    QString errorMessage;
    const CalendarImport::Workbook workbook = CalendarImport::parseWorkbook(
        data,
        &errorMessage
        );
    if (workbook.worksheets.isEmpty())
    {
        return std::unexpected(
            errorMessage.trimmed().isEmpty()
                ? QObject::tr("The workbook could not be read.")
                : errorMessage
            );
    }

    const classmngr::engine::ScheduleWorkbookLayout layout = toLayout(workbook);
    const auto interpreted =
        classmngr::engine::ScheduleWorkbookInterpreter::interpret(
            layout,
            toEngineKind(kind),
            [&isCancelled] {
                return isCancelled && isCancelled();
            }
            );
    if (!interpreted)
    {
        return std::unexpected(fromUtf8(interpreted.error().message));
    }
    return fromEngineWorkbook(*interpreted);
}

QString normalizedScheduleImportUserName(const QString& value)
{
    const QString normalized = value.normalized(
        QString::NormalizationForm_KC
        ).toCaseFolded();
    QString result;
    bool pendingSpace = false;
    for (const QChar character : normalized)
    {
        if (character.isLetterOrNumber())
        {
            if (pendingSpace && !result.isEmpty())
            {
                result.append(QLatin1Char(' '));
            }
            result.append(character);
            pendingSpace = false;
        }
        else if (character.isSpace())
        {
            pendingSpace = true;
        }
    }
    return result.trimmed();
}

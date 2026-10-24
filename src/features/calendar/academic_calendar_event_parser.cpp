#include "academic_calendar_event_parser.h"

#include "calendar_workbook_reader.h"
#include "next/application/calendar_event_import_signature.h"

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <unordered_set>

#include <QDate>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace CalendarImport
{
namespace
{
struct CalendarEventImportSignatureHash final
{
    [[nodiscard]] std::size_t operator()(
        const ClassMngr::Next::Application::CalendarEventImportSignature& key
        ) const noexcept
    {
        return std::hash<std::u16string>{}(key.value());
    }
};

constexpr int LegendColumn = 26;
constexpr int CalendarGridRows = 6;
constexpr int CalendarGridColumns = 7;
constexpr int NotesSearchRows = 8;

struct MonthBlock
{
    int year = 0;
    int month = 0;
    int row = 0;
    int column = 0;
};

bool isMeaningfulFill(
    const QString& color
    )
{
    const QString normalized =
        normalizedColor(color);

    return !normalized.isEmpty()
        && normalized != QStringLiteral("FFFFFF");
}

bool isMeaningfulFontColor(
    const QString& color
    )
{
    const QString normalized =
        normalizedColor(color);

    return !normalized.isEmpty()
        && normalized != QStringLiteral("000000")
        && normalized != QStringLiteral("FFFFFF");
}

QString comparableFontColor(
    const QString& color
    )
{
    const QString normalized =
        normalizedColor(color);

    return normalized.isEmpty()
        ? QStringLiteral("000000")
        : normalized;
}

QString cellText(
    const QHash<CellPosition, const Cell*>& cellsByPosition,
    int row,
    int column
    )
{
    const Cell* cell =
        cellsByPosition.value(CellPosition{row, column}, nullptr);
    return cell
        ? cell->value.trimmed()
        : QString{};
}

Style cellStyle(
    const WorkbookMetadata& metadata,
    const Cell& cell
    )
{
    if (cell.style < 0 || cell.style >= metadata.styles.size())
    {
        return {};
    }

    return metadata.styles[cell.style];
}

int calendarYear(
    const Worksheet& worksheet
    )
{
    static const QRegularExpression yearPattern(
        QStringLiteral("(20\\d{2})")
        );

    for (const Cell& cell : worksheet.cells)
    {
        const QRegularExpressionMatch match =
            yearPattern.match(cell.value);
        if (match.hasMatch())
        {
            return match.captured(1).toInt();
        }
    }

    return QDate::currentDate().year();
}

int monthNumber(
    const QString& value
    )
{
    static const QStringList months{
        QStringLiteral("JANUARY"),
        QStringLiteral("FEBRUARY"),
        QStringLiteral("MARCH"),
        QStringLiteral("APRIL"),
        QStringLiteral("MAY"),
        QStringLiteral("JUNE"),
        QStringLiteral("JULY"),
        QStringLiteral("AUGUST"),
        QStringLiteral("SEPTEMBER"),
        QStringLiteral("OCTOBER"),
        QStringLiteral("NOVEMBER"),
        QStringLiteral("DECEMBER")
    };

    const QString normalized =
        value.trimmed().toUpper();

    int index =
        months.indexOf(normalized);

    if (index < 0 && normalized.size() >= 3)
    {
        for (int month = 0; month < months.size(); ++month)
        {
            if (months[month].startsWith(normalized.left(3)))
            {
                index = month;
                break;
            }
        }
    }

    return index >= 0 ? index + 1 : 0;
}

QVector<MonthBlock> monthBlocks(
    const Worksheet& worksheet
    )
{
    QVector<MonthBlock> blocks;
    const int year =
        calendarYear(worksheet);

    for (const Cell& cell : worksheet.cells)
    {
        const int month =
            monthNumber(cell.value);

        if (month > 0)
        {
            blocks.append(
                MonthBlock{
                    year,
                    month,
                    cell.position.row,
                    cell.position.column
                }
                );
        }
    }

    std::sort(
        blocks.begin(),
        blocks.end(),
        [](const MonthBlock& lhs, const MonthBlock& rhs)
        {
            return lhs.month < rhs.month;
        }
        );

    return blocks;
}

QDate gridDate(
    const MonthBlock& block,
    int gridStartRow,
    int row,
    int column
    )
{
    const QDate firstOfMonth(
        block.year,
        block.month,
        1
        );

    if (!firstOfMonth.isValid())
    {
        return {};
    }

    const QDate firstGridDate =
        firstOfMonth.addDays(
            -(firstOfMonth.dayOfWeek() - Qt::Monday)
            );

    return firstGridDate.addDays(
        (row - gridStartRow) * 7
        + column
        - block.column
        );
}

bool hasDisplayedDayInRow(
    const QHash<CellPosition, const Cell*>& cellsByPosition,
    const MonthBlock& block,
    int row
    )
{
    for (int column = block.column;
         column < block.column + CalendarGridColumns;
         ++column)
    {
        const Cell* cell =
            cellsByPosition.value(CellPosition{row, column}, nullptr);
        if (!cell)
        {
            continue;
        }

        bool dayOk = false;
        const double displayedDay =
            cell->value
                .toDouble(&dayOk);

        if (dayOk && displayedDay > 0)
        {
            return true;
        }
    }

    return false;
}

bool cellDisplaysDay(
    const QHash<CellPosition, const Cell*>& cellsByPosition,
    int row,
    int column,
    int day
    )
{
    const Cell* cell =
        cellsByPosition.value(CellPosition{row, column}, nullptr);
    if (!cell)
    {
        return false;
    }

    bool dayOk = false;
    const int displayedDay =
        cell->value
            .toDouble(&dayOk);

    return dayOk && displayedDay == day;
}

int gridStartRow(
    const QHash<CellPosition, const Cell*>& cellsByPosition,
    const MonthBlock& block
    )
{
    const int assumedStartRow =
        block.row + 2;
    const QDate firstOfMonth(
        block.year,
        block.month,
        1
        );

    if (!firstOfMonth.isValid())
    {
        return assumedStartRow;
    }

    const int firstDayColumn =
        block.column
        + firstOfMonth.dayOfWeek()
        - Qt::Monday;

    if (
        !hasDisplayedDayInRow(cellsByPosition, block, assumedStartRow)
        && cellDisplaysDay(
            cellsByPosition,
            assumedStartRow + 1,
            firstDayColumn,
            1
            )
        )
    {
        return assumedStartRow + 1;
    }

    return assumedStartRow;
}

QString cleanedLegendLabel(
    QString label
    )
{
    const int parenthesisIndex =
        label.indexOf(QLatin1Char('('));

    if (parenthesisIndex >= 0)
    {
        label.truncate(parenthesisIndex);
    }

    return label.simplified();
}

bool isWeekendLegend(
    const QString& label
    )
{
    return cleanedLegendLabel(label).compare(
        QStringLiteral("Weekend"),
        Qt::CaseInsensitive
        ) == 0;
}

bool shouldIgnoreLegend(
    const QString& label
    )
{
    const QString normalized =
        cleanedLegendLabel(label).toLower();

    return normalized == QStringLiteral("weekend")
        || normalized == QStringLiteral("creo fixed days")
        || normalized == QStringLiteral("creo workshop")
        || normalized == QStringLiteral("date confirmed")
        || normalized == QStringLiteral("date not confirmed");
}

QString eventTypeForLegend(
    const QString& label
    )
{
    const QString normalized =
        cleanedLegendLabel(label).toLower();

    if (
        normalized == QStringLiteral("red day")
        || normalized == QStringLiteral("substitute / special red day")
        || normalized == QStringLiteral("dyb fixed days")
        )
    {
        return QStringLiteral("Holiday");
    }

    if (normalized == QStringLiteral("dyb workshop"))
    {
        return QStringLiteral("Workshop");
    }

    if (
        normalized.startsWith(QStringLiteral("cms"))
        || normalized.contains(QStringLiteral("parent meetings"))
        )
    {
        return QStringLiteral("CM");
    }

    return QStringLiteral("Other");
}

QHash<QString, QString> fillLegend(
    const WorkbookMetadata& metadata,
    const Worksheet& worksheet
    )
{
    QHash<QString, QString> legend;

    for (const Cell& cell : worksheet.cells)
    {
        if (cell.position.column != LegendColumn)
        {
            continue;
        }

        const Style style =
            cellStyle(metadata, cell);
        if (!isMeaningfulFill(style.fillColor))
        {
            continue;
        }

        legend.insert(
            normalizedColor(style.fillColor),
            cell.value.simplified()
            );
    }

    return legend;
}

QHash<QString, QString> fontLegend(
    const WorkbookMetadata& metadata,
    const Worksheet& worksheet
    )
{
    QHash<QString, QString> legend;

    for (const Cell& cell : worksheet.cells)
    {
        if (cell.position.column != LegendColumn)
        {
            continue;
        }

        const Style style =
            cellStyle(metadata, cell);

        if (!isMeaningfulFontColor(style.fontColor))
        {
            continue;
        }

        legend.insert(
            normalizedColor(style.fontColor),
            cell.value.simplified()
            );
    }

    return legend;
}

Style weekendLegendStyle(
    const WorkbookMetadata& metadata,
    const Worksheet& worksheet
    )
{
    for (const Cell& cell : worksheet.cells)
    {
        if (
            cell.position.column == LegendColumn
            && isWeekendLegend(cell.value)
            )
        {
            return cellStyle(metadata, cell);
        }
    }

    return {};
}

QList<QDate> noteDates(
    int year,
    int baseMonth,
    const QString& dayStartText,
    const QString& separator,
    const QString& dayEndText,
    const QString& monthOverride
    )
{
    bool ok = false;
    const int firstDay =
        dayStartText.toInt(&ok);

    if (!ok)
    {
        return {};
    }

    int month =
        baseMonth;
    int noteYear =
        year;

    if (!monthOverride.trimmed().isEmpty())
    {
        const int overrideMonth =
            monthNumber(
                monthOverride.left(3).toUpper()
                );

        if (overrideMonth > 0)
        {
            month =
                overrideMonth;

            if (baseMonth == 12 && month == 1)
            {
                ++noteYear;
            }
            else if (baseMonth == 1 && month == 12)
            {
                --noteYear;
            }
        }
    }

    QList<QDate> dates;
    dates.append(
        QDate(noteYear, month, firstDay)
        );

    if (!dayEndText.trimmed().isEmpty())
    {
        bool endOk = false;
        const int lastDay =
            dayEndText.toInt(&endOk);

        if (endOk)
        {
            if (separator == QStringLiteral("-"))
            {
                for (int day = firstDay + 1; day <= lastDay; ++day)
                {
                    dates.append(
                        QDate(noteYear, month, day)
                        );
                }
            }
            else if (separator == QStringLiteral("/"))
            {
                dates.append(
                    QDate(noteYear, month, lastDay)
                    );
            }
        }
    }

    dates.erase(
        std::remove_if(
            dates.begin(),
            dates.end(),
            [](const QDate& date)
            {
                return !date.isValid();
            }
            ),
        dates.end()
        );

    return dates;
}

void addNoteEntries(
    const MonthBlock& block,
    const QString& notes,
    QHash<QDate, QStringList>* titlesByDate,
    QSet<QDate>* cancelledDates
    )
{
    static const QRegularExpression entryPattern(
        QStringLiteral(
            "^\\s*(\\d{1,2})(?:\\s*([-/])\\s*(\\d{1,2}))?"
            "(?:\\s+([A-Za-z]{3,9}))?(?:\\s*\\?\\?)?\\s+-\\s+(.+)\\s*$"
            )
        );

    const QStringList lines =
        notes.split(
            QLatin1Char('\n'),
            Qt::SkipEmptyParts
            );

    for (const QString& rawLine : lines)
    {
        const QString line =
            rawLine.simplified();
        const QRegularExpressionMatch match =
            entryPattern.match(line);

        if (!match.hasMatch())
        {
            continue;
        }

        const QList<QDate> dates =
            noteDates(
                block.year,
                block.month,
                match.captured(1),
                match.captured(2),
                match.captured(3),
                match.captured(4)
                );
        const QString title =
            match.captured(5).simplified();
        const bool cancelled =
            title.contains(
                QStringLiteral("CANCELLED"),
                Qt::CaseInsensitive
                );

        for (const QDate& date : dates)
        {
            if (cancelled)
            {
                cancelledDates->insert(date);
                continue;
            }

            (*titlesByDate)[date].append(title);
        }
    }
}

QHash<QDate, QStringList> noteTitlesByDate(
    const QVector<MonthBlock>& blocks,
    const QHash<CellPosition, const Cell*>& cellsByPosition,
    QSet<QDate>* cancelledDates
    )
{
    QHash<QDate, QStringList> titlesByDate;

    for (const MonthBlock& block : blocks)
    {
        for (int row = block.row + 8;
             row <= block.row + 8 + NotesSearchRows;
             ++row)
        {
            const QString text =
                cellText(
                    cellsByPosition,
                    row,
                    block.column
                    );

            if (!text.contains(QLatin1Char('-')))
            {
                continue;
            }

            addNoteEntries(
                block,
                text,
                &titlesByDate,
                cancelledDates
                );
            break;
        }
    }

    return titlesByDate;
}

QStringList normalizedCampusCodes(
    const QStringList& campusCodes
    )
{
    QStringList codes;

    for (const QString& code : campusCodes)
    {
        const QString normalized =
            code.trimmed();

        if (!normalized.isEmpty())
        {
            codes.append(normalized);
        }
    }

    codes.removeDuplicates();
    return codes;
}

bool containsCampusCode(
    const QString& text,
    const QString& code
    )
{
    if (code.trimmed().isEmpty())
    {
        return false;
    }

    const QRegularExpression pattern(
        QStringLiteral("(^|[^A-Z0-9])%1([^A-Z0-9]|$)")
            .arg(QRegularExpression::escape(code.trimmed())),
        QRegularExpression::CaseInsensitiveOption
        );

    return pattern.match(text).hasMatch();
}

QStringList campusCodesInNote(
    const QString& note,
    const QStringList& knownCampusCodes
    )
{
    QStringList codes;

    for (const QString& code : knownCampusCodes)
    {
        if (containsCampusCode(note, code))
        {
            codes.append(code.trimmed());
        }
    }

    codes.removeDuplicates();
    return codes;
}

QString titleWithCampusCodes(
    const QString& title,
    const QString& note,
    const QStringList& knownCampusCodes
    )
{
    QString normalizedTitle =
        title.simplified();
    QStringList codes =
        campusCodesInNote(note, knownCampusCodes);

    codes.erase(
        std::remove_if(
            codes.begin(),
            codes.end(),
            [&normalizedTitle](const QString& code)
            {
                return containsCampusCode(
                    normalizedTitle,
                    code
                    );
            }
            ),
        codes.end()
        );

    if (codes.isEmpty())
    {
        return normalizedTitle;
    }

    return QStringLiteral("%1 (%2)")
        .arg(
            normalizedTitle,
            codes.join(QStringLiteral(", "))
            );
}

}

ClassMngr::Next::Application::CalendarEventImportSignature
calendarEventImportSignature(
    const CalendarEvent& event
    )
{
    return ClassMngr::Next::Application::CalendarEventImportSignature::
        fromNormalizedFields({
            .simplifiedTitle = event.title.simplified().toStdU16String(),
            .normalizedEventType = normalizedCalendarEventType(
                event.eventType
                ).toStdU16String(),
            .startDateIso = event.startDate
                .toString(Qt::ISODate)
                .toStdU16String(),
            .endDateIso = event.endDate
                .toString(Qt::ISODate)
                .toStdU16String(),
            .allDay = event.allDay,
            .normalizedTimeStatus = normalizedCalendarEventTimeStatus(
                event.timeStatus
                ).toStdU16String()
        });
}

CalendarEvent calendarEvent(
    const QDate& date,
    const QString& title,
    const QString& eventType
    )
{
    CalendarEvent event;
    event.title =
        title.simplified();
    event.eventType =
        normalizedCalendarEventType(eventType);
    event.allDay =
        event.eventType == QStringLiteral("Holiday")
        || event.eventType == QStringLiteral("Vacation")
        || event.eventType == QStringLiteral("Meeting");
    event.timeStatus =
        event.allDay
            ? QStringLiteral("Timed")
            : QStringLiteral("Unknown");
    event.startDate =
        date;
    event.endDate =
        date;

    return event;
}

ParsedCalendarImport parseCalendarEventsFromWorksheet(
    const WorkbookMetadata& metadata,
    const Worksheet& worksheet,
    const QStringList& campusCodes
    )
{
    ParsedCalendarImport result;
    const QStringList knownCampusCodes =
        normalizedCampusCodes(campusCodes);
    const QVector<MonthBlock> blocks =
        monthBlocks(worksheet);

    QHash<CellPosition, const Cell*> cellsByPosition;
    for (const Cell& cell : worksheet.cells)
    {
        cellsByPosition.insert(
            cell.position,
            &cell
            );
    }

    QSet<QDate> cancelledDates;
    const QHash<QDate, QStringList> titlesByDate =
        noteTitlesByDate(
            blocks,
            cellsByPosition,
            &cancelledDates
            );
    const QHash<QString, QString> labelsByFill =
        fillLegend(metadata, worksheet);
    const QHash<QString, QString> labelsByFont =
        fontLegend(metadata, worksheet);
    const Style weekendStyle =
        weekendLegendStyle(metadata, worksheet);
    const QString weekendFill =
        normalizedColor(weekendStyle.fillColor);
    const QString weekendFont =
        comparableFontColor(weekendStyle.fontColor);
    std::unordered_set<
        ClassMngr::Next::Application::CalendarEventImportSignature,
        CalendarEventImportSignatureHash
        > emitted;

    for (const MonthBlock& block : blocks)
    {
        const int startRow =
            gridStartRow(
                cellsByPosition,
                block
                );

        for (int row = startRow;
             row < startRow + CalendarGridRows;
             ++row)
        {
            for (int column = block.column;
                 column < block.column + CalendarGridColumns;
                 ++column)
            {
                const Cell* cell =
                    cellsByPosition.value(
                        CellPosition{row, column},
                        nullptr
                        );

                if (!cell || cell->value.trimmed().isEmpty())
                {
                    continue;
                }

                bool dayOk = false;
                const int displayedDay =
                    cell->value.toDouble(&dayOk);

                if (!dayOk || displayedDay <= 0)
                {
                    continue;
                }

                const QDate date =
                    gridDate(
                        block,
                        startRow,
                        row,
                        column
                        );

                if (
                    !date.isValid()
                    || date.day() != displayedDay
                    || cancelledDates.contains(date)
                    )
                {
                    continue;
                }

                const Style style =
                    cellStyle(metadata, *cell);
                QStringList labels;

                const QString fill =
                    normalizedColor(style.fillColor);
                const bool isWeekendFill =
                    isMeaningfulFill(weekendFill)
                    && fill == weekendFill;
                if (labelsByFill.contains(fill))
                {
                    labels.append(
                        labelsByFill.value(fill)
                        );
                }

                const QString font =
                    normalizedColor(style.fontColor);
                const bool matchesWeekendFont =
                    comparableFontColor(font) == weekendFont;
                const bool shouldMatchFont =
                    (
                        isWeekendFill
                        && !matchesWeekendFont
                    )
                    || (
                        date.dayOfWeek() >= Qt::Monday
                        && date.dayOfWeek() <= Qt::Friday
                        && !isMeaningfulFill(fill)
                        && isMeaningfulFontColor(font)
                    );
                if (
                    labelsByFont.contains(font)
                    && shouldMatchFont
                    )
                {
                    labels.append(
                        labelsByFont.value(font)
                        );
                }

                labels.removeDuplicates();

                for (const QString& label : labels)
                {
                    if (shouldIgnoreLegend(label))
                    {
                        ++result.skippedCount;
                        continue;
                    }

                    const QString eventType =
                        eventTypeForLegend(label);
                    QStringList titles =
                        titlesByDate.value(date);

                    if (titles.isEmpty())
                    {
                        titles.append(
                            cleanedLegendLabel(label)
                            );
                    }

                    for (const QString& title : titles)
                    {
                        const QString importTitle =
                            titleWithCampusCodes(
                                title,
                                cell->note,
                                knownCampusCodes
                                );
                        const CalendarEvent event =
                            calendarEvent(
                                date,
                                importTitle,
                                eventType
                                );
                        const auto signature =
                            calendarEventImportSignature(event);

                        if (emitted.contains(signature))
                        {
                            continue;
                        }

                        emitted.insert(signature);
                        result.events.append(event);
                    }
                }
            }
        }
    }

    return result;
}

template<typename Archive>
static ParsedCalendarImport parseCalendarEventsFromWorkbookSource(
    Archive& data,
    const QStringList& campusCodes,
    QString* errorMessage,
    CalendarWorkbookReadCounts* readCounts
    )
{
    struct RetainedRow
    {
        QHash<CellPosition, Cell> cells;
    };

    struct GridBlockState
    {
        int startRow = 0;
        int nextRow = 0;
        bool resolved = false;
    };

    ParsedCalendarImport result;
    CalendarWorkbookReadCounts counts;
    const QStringList knownCampusCodes =
        normalizedCampusCodes(campusCodes);
    bool firstWorksheet = true;

    const bool succeeded = forEachWorksheetRows(
        data,
        [&](
            const WorkbookMetadata& metadata,
            const Worksheet& worksheet,
            const WorksheetRowPass& discoveryPass,
            const WorksheetRowPass& extractionPass
            )
        {
            ++counts.worksheetCount;
            if (!firstWorksheet)
            {
                const bool discovered = discoveryPass(
                    [&counts](const QVector<Cell>& row)
                    {
                        counts.cellCount += static_cast<int>(row.size());
                        return true;
                    }
                    );
                if (discovered)
                {
                    counts.mergedRangeCount += static_cast<int>(
                        worksheet.mergedRanges.size()
                        );
                }
                return discovered && extractionPass({});
            }
            firstWorksheet = false;
            counts.sharedStringCount = static_cast<int>(
                metadata.sharedStrings.size()
                );
            counts.styleCount = static_cast<int>(metadata.styles.size());

            static const QRegularExpression yearPattern(
                QStringLiteral("(20\\d{2})")
                );
            bool foundYear = false;
            int year = QDate::currentDate().year();
            QVector<MonthBlock> blocks;
            QSet<CellPosition> noteSearchPositions;
            QHash<CellPosition, Cell> noteCells;
            QHash<QString, QString> labelsByFill;
            QHash<QString, QString> labelsByFont;
            Style weekendStyle;
            bool foundWeekendStyle = false;

            const bool discovered = discoveryPass(
                [&](const QVector<Cell>& row)
                {
                    counts.cellCount += static_cast<int>(row.size());
                    for (const Cell& cell : row)
                    {
                        if (!foundYear)
                        {
                            const QRegularExpressionMatch match =
                                yearPattern.match(cell.value);
                            if (match.hasMatch())
                            {
                                year = match.captured(1).toInt();
                                foundYear = true;
                            }
                        }

                        const int month = monthNumber(cell.value);
                        if (month > 0)
                        {
                            const MonthBlock block{
                                0,
                                month,
                                cell.position.row,
                                cell.position.column
                            };
                            blocks.append(block);
                            for (
                                int noteRow = block.row + 8;
                                noteRow <= block.row + 8 + NotesSearchRows;
                                ++noteRow
                                )
                            {
                                noteSearchPositions.insert(
                                    CellPosition{noteRow, block.column}
                                    );
                            }
                        }

                        if (cell.position.column == LegendColumn)
                        {
                            const Style style = cellStyle(metadata, cell);
                            if (isMeaningfulFill(style.fillColor))
                            {
                                labelsByFill.insert(
                                    normalizedColor(style.fillColor),
                                    cell.value.simplified()
                                    );
                            }
                            if (isMeaningfulFontColor(style.fontColor))
                            {
                                labelsByFont.insert(
                                    normalizedColor(style.fontColor),
                                    cell.value.simplified()
                                    );
                            }
                            if (
                                !foundWeekendStyle
                                && isWeekendLegend(cell.value)
                                )
                            {
                                weekendStyle = style;
                                foundWeekendStyle = true;
                            }
                        }

                        if (noteSearchPositions.contains(cell.position))
                        {
                            noteCells.insert(cell.position, cell);
                        }
                    }
                    return true;
                }
                );
            if (!discovered)
            {
                return false;
            }
            counts.mergedRangeCount += static_cast<int>(
                worksheet.mergedRanges.size()
                );

            for (MonthBlock& block : blocks)
            {
                block.year = year;
            }
            std::sort(
                blocks.begin(),
                blocks.end(),
                [](const MonthBlock& lhs, const MonthBlock& rhs)
                {
                    return lhs.month < rhs.month;
                }
                );

            QHash<CellPosition, const Cell*> noteCellPointers;
            for (auto note = noteCells.cbegin();
                 note != noteCells.cend();
                 ++note)
            {
                noteCellPointers.insert(note.key(), &note.value());
            }
            QSet<QDate> cancelledDates;
            const QHash<QDate, QStringList> titlesByDate =
                noteTitlesByDate(
                    blocks,
                    noteCellPointers,
                    &cancelledDates
                    );

            QVector<GridBlockState> blockStates(blocks.size());
            QVector<QVector<CalendarEvent>> eventsByBlock(blocks.size());
            QVector<int> skippedByBlock(blocks.size(), 0);
            QMap<int, QVector<int>> blocksByActivationRow;
            QHash<int, int> rowUseCounts;
            QHash<int, QSet<int>> columnsNeededByRow;
            for (int blockIndex = 0;
                 blockIndex < blocks.size();
                 ++blockIndex)
            {
                const MonthBlock& block = blocks[blockIndex];
                blocksByActivationRow[block.row + 3].append(blockIndex);
                for (int row = block.row + 2; row <= block.row + 8; ++row)
                {
                    ++rowUseCounts[row];
                    QSet<int>& columns = columnsNeededByRow[row];
                    for (int column = block.column;
                         column < block.column + CalendarGridColumns;
                         ++column)
                    {
                        columns.insert(column);
                    }
                }
            }

            QHash<int, RetainedRow> retainedRows;
            QList<int> activeBlocks;
            int lastSeenRow = 0;
            const QString weekendFill =
                normalizedColor(weekendStyle.fillColor);
            const QString weekendFont =
                comparableFontColor(weekendStyle.fontColor);

            const auto releaseRowUse =
                [&](int row)
                {
                    const int remaining = rowUseCounts.value(row) - 1;
                    if (remaining <= 0)
                    {
                        rowUseCounts.remove(row);
                        columnsNeededByRow.remove(row);
                        retainedRows.remove(row);
                    }
                    else
                    {
                        rowUseCounts.insert(row, remaining);
                    }
                };

            const auto rowCellsAt =
                [&retainedRows](int row) -> const QHash<CellPosition, Cell>*
                {
                    const auto retained = retainedRows.constFind(row);
                    return retained == retainedRows.cend()
                        ? nullptr
                        : &retained->cells;
                };

            const auto displayedDayInRow =
                [&rowCellsAt](
                    int row,
                    const MonthBlock& block
                    )
                {
                    const QHash<CellPosition, Cell>* cells = rowCellsAt(row);
                    if (!cells)
                    {
                        return false;
                    }
                    for (int column = block.column;
                         column < block.column + CalendarGridColumns;
                         ++column)
                    {
                        const auto found = cells->constFind(
                            CellPosition{row, column}
                            );
                        if (found == cells->cend())
                        {
                            continue;
                        }
                        const Cell* cell = &found.value();
                        bool dayOk = false;
                        const double displayedDay =
                            cell->value.toDouble(&dayOk);
                        if (dayOk && displayedDay > 0)
                        {
                            return true;
                        }
                    }
                    return false;
                };

            const auto cellDisplaysDayAt =
                [&rowCellsAt](int row, int column, int day)
                {
                    const QHash<CellPosition, Cell>* cells = rowCellsAt(row);
                    if (!cells)
                    {
                        return false;
                    }
                    const auto cell = cells->constFind(
                        CellPosition{row, column}
                        );
                    if (cell == cells->cend())
                    {
                        return false;
                    }
                    bool dayOk = false;
                    const int displayedDay =
                        cell->value.toDouble(&dayOk);
                    return dayOk && displayedDay == day;
                };

            const auto processBlockRow =
                [&](int blockIndex, int rowNumber)
                {
                    const MonthBlock& block = blocks[blockIndex];
                    const QHash<CellPosition, Cell>* cells =
                        rowCellsAt(rowNumber);
                    if (!cells)
                    {
                        return;
                    }

                    for (int column = block.column;
                         column < block.column + CalendarGridColumns;
                         ++column)
                    {
                        const auto found = cells->constFind(
                            CellPosition{rowNumber, column}
                            );
                        if (found == cells->cend())
                        {
                            continue;
                        }
                        const Cell& cell = found.value();
                        if (cell.value.trimmed().isEmpty())
                        {
                            continue;
                        }

                        bool dayOk = false;
                        const double displayedDay =
                            cell.value.toDouble(&dayOk);
                        if (!dayOk || displayedDay <= 0)
                        {
                            continue;
                        }

                        const QDate date = gridDate(
                            block,
                            blockStates[blockIndex].startRow,
                            rowNumber,
                            column
                            );
                        if (
                            !date.isValid()
                            || date.day() != displayedDay
                            || cancelledDates.contains(date)
                            )
                        {
                            continue;
                        }

                        const Style style = cellStyle(metadata, cell);
                        QStringList labels;
                        const QString fill =
                            normalizedColor(style.fillColor);
                        const bool isWeekendFill =
                            isMeaningfulFill(weekendFill)
                            && fill == weekendFill;
                        if (labelsByFill.contains(fill))
                        {
                            labels.append(labelsByFill.value(fill));
                        }

                        const QString font =
                            normalizedColor(style.fontColor);
                        const bool matchesWeekendFont =
                            comparableFontColor(font) == weekendFont;
                        const bool shouldMatchFont =
                            (
                                isWeekendFill
                                && !matchesWeekendFont
                                )
                            || (
                                date.dayOfWeek() >= Qt::Monday
                                && date.dayOfWeek() <= Qt::Friday
                                && !isMeaningfulFill(fill)
                                && isMeaningfulFontColor(font)
                                );
                        if (
                            labelsByFont.contains(font)
                            && shouldMatchFont
                            )
                        {
                            labels.append(labelsByFont.value(font));
                        }
                        labels.removeDuplicates();

                        for (const QString& label : labels)
                        {
                            if (shouldIgnoreLegend(label))
                            {
                                ++skippedByBlock[blockIndex];
                                continue;
                            }

                            const QString eventType =
                                eventTypeForLegend(label);
                            QStringList titles = titlesByDate.value(date);
                            if (titles.isEmpty())
                            {
                                titles.append(cleanedLegendLabel(label));
                            }

                            for (const QString& title : titles)
                            {
                                eventsByBlock[blockIndex].append(
                                    calendarEvent(
                                        date,
                                        titleWithCampusCodes(
                                            title,
                                            cell.note,
                                            knownCampusCodes
                                            ),
                                        eventType
                                        )
                                    );
                            }
                        }
                    }
                };

            const auto processBlockThrough =
                [&](int blockIndex, int latestRow)
                {
                    GridBlockState& state = blockStates[blockIndex];
                    const int finalGridRow = state.startRow + CalendarGridRows - 1;
                    while (state.nextRow <= finalGridRow && state.nextRow <= latestRow)
                    {
                        processBlockRow(blockIndex, state.nextRow);
                        releaseRowUse(state.nextRow);
                        ++state.nextRow;
                    }
                };

            const auto resolveBlock =
                [&](int blockIndex, int latestRow)
                {
                    const MonthBlock& block = blocks[blockIndex];
                    const int assumedStartRow = block.row + 2;
                    const QDate firstOfMonth(block.year, block.month, 1);
                    int startRow = assumedStartRow;
                    if (firstOfMonth.isValid())
                    {
                        const int firstDayColumn =
                            block.column
                            + firstOfMonth.dayOfWeek()
                            - Qt::Monday;
                        if (
                            !displayedDayInRow(assumedStartRow, block)
                            && cellDisplaysDayAt(
                                block.row + 3,
                                firstDayColumn,
                                1
                                )
                            )
                        {
                            startRow = assumedStartRow + 1;
                        }
                    }

                    GridBlockState& state = blockStates[blockIndex];
                    state.startRow = startRow;
                    state.nextRow = startRow;
                    state.resolved = true;

                    const int unusedRow = startRow == assumedStartRow
                        ? block.row + 8
                        : block.row + 2;
                    releaseRowUse(unusedRow);
                    processBlockThrough(blockIndex, latestRow);
                };

            const auto consumeRows = extractionPass(
                [&](const QVector<Cell>& row)
                {
                    if (row.isEmpty())
                    {
                        return true;
                    }
                    const int rowNumber = row.first().position.row;
                    lastSeenRow = rowNumber;
                    if (rowUseCounts.value(rowNumber) > 0)
                    {
                        RetainedRow& retained = retainedRows[rowNumber];
                        const QSet<int>& neededColumns =
                            columnsNeededByRow.value(rowNumber);
                        for (const Cell& cell : row)
                        {
                            if (neededColumns.contains(cell.position.column))
                            {
                                retained.cells.insert(cell.position, cell);
                            }
                        }
                    }

                    for (int blockIndex : activeBlocks)
                    {
                        processBlockThrough(blockIndex, rowNumber);
                    }

                    while (
                        !blocksByActivationRow.isEmpty()
                        && blocksByActivationRow.firstKey() <= rowNumber
                        )
                    {
                        const QVector<int> dueBlocks =
                            blocksByActivationRow.take(
                                blocksByActivationRow.firstKey()
                                );
                        for (int blockIndex : dueBlocks)
                        {
                            resolveBlock(blockIndex, rowNumber);
                            if (
                                blockStates[blockIndex].nextRow
                                <= blockStates[blockIndex].startRow
                                    + CalendarGridRows - 1
                                )
                            {
                                activeBlocks.append(blockIndex);
                            }
                        }
                    }

                    for (qsizetype index = activeBlocks.size() - 1;
                         index >= 0;
                         --index)
                    {
                        const GridBlockState& state =
                            blockStates[activeBlocks[index]];
                        if (
                            state.nextRow
                            > state.startRow + CalendarGridRows - 1
                            )
                        {
                            activeBlocks.removeAt(index);
                        }
                    }
                    return true;
                }
                );
            if (!consumeRows)
            {
                return false;
            }

            while (!blocksByActivationRow.isEmpty())
            {
                const int activationRow = blocksByActivationRow.firstKey();
                const QVector<int> dueBlocks =
                    blocksByActivationRow.take(activationRow);
                for (int blockIndex : dueBlocks)
                {
                    resolveBlock(blockIndex, lastSeenRow);
                }
            }

            std::unordered_set<
                ClassMngr::Next::Application::CalendarEventImportSignature,
                CalendarEventImportSignatureHash
                > emitted;
            for (int blockIndex = 0;
                 blockIndex < blocks.size();
                 ++blockIndex)
            {
                for (int count = 0;
                     count < skippedByBlock[blockIndex];
                     ++count)
                {
                    ++result.skippedCount;
                }
                for (const CalendarEvent& event : eventsByBlock[blockIndex])
                {
                    const auto signature = calendarEventImportSignature(event);
                    if (emitted.contains(signature))
                    {
                        continue;
                    }
                    emitted.insert(signature);
                    result.events.append(event);
                }
            }

            return true;
        },
        errorMessage
        );

    if (readCounts)
    {
        *readCounts = counts;
    }
    if (!succeeded)
    {
        return {};
    }
    return result;
}

ParsedCalendarImport parseCalendarEventsFromWorkbookBytes(
    const QByteArray& data,
    const QStringList& campusCodes,
    QString* errorMessage,
    CalendarWorkbookReadCounts* readCounts
    )
{
    return parseCalendarEventsFromWorkbookSource(
        data,
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
    return parseCalendarEventsFromWorkbookSource(
        device,
        campusCodes,
        errorMessage,
        readCounts
        );
}

ParsedCalendarImport parseCalendarEventsFromWorkbook(
    const Workbook& workbook,
    const QStringList& campusCodes
    )
{
    if (workbook.worksheets.isEmpty())
    {
        return {};
    }

    const WorkbookMetadata metadata{
        workbook.sharedStrings,
        workbook.styles
    };
    return parseCalendarEventsFromWorksheet(
        metadata,
        workbook.worksheets.constFirst(),
        campusCodes
        );
}

}

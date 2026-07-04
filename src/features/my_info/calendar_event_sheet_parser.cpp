#include "calendar_event_sheet_parser.h"

#include "calendar_import_workbook.h"

#include <algorithm>
#include <array>

#include <QDate>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace CalendarImport
{
namespace
{
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

QString cellText(
    const QHash<int, Cell>& cellsByPosition,
    int row,
    int column
    )
{
    return cellsByPosition
        .value(row * 100 + column)
        .value
        .trimmed();
}

Style cellStyle(
    const Workbook& workbook,
    const Cell& cell
    )
{
    if (cell.style < 0 || cell.style >= workbook.styles.size())
    {
        return {};
    }

    return workbook.styles[cell.style];
}

int calendarYear(
    const Workbook& workbook
    )
{
    static const QRegularExpression yearPattern(
        QStringLiteral("(20\\d{2})")
        );

    for (const Cell& cell : workbook.cells)
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
    const Workbook& workbook
    )
{
    QVector<MonthBlock> blocks;
    const int year =
        calendarYear(workbook);

    for (const Cell& cell : workbook.cells)
    {
        const int month =
            monthNumber(cell.value);

        if (month > 0)
        {
            blocks.append(
                MonthBlock{
                    year,
                    month,
                    cell.row,
                    cell.column
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
        (row - (block.row + 2)) * 7
        + column
        - block.column
        );
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

bool shouldIgnoreLegend(
    const QString& label
    )
{
    const QString normalized =
        cleanedLegendLabel(label).toLower();

    return normalized == QStringLiteral("weekend")
        || normalized == QStringLiteral("new semester")
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
    const Workbook& workbook
    )
{
    QHash<QString, QString> legend;

    for (const Cell& cell : workbook.cells)
    {
        if (cell.column != LegendColumn)
        {
            continue;
        }

        const Style style =
            cellStyle(workbook, cell);
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
    const Workbook& workbook
    )
{
    QHash<QString, QString> legend;

    for (const Cell& cell : workbook.cells)
    {
        if (cell.column != LegendColumn)
        {
            continue;
        }

        const Style style =
            cellStyle(workbook, cell);

        if (
            isMeaningfulFill(style.fillColor)
            || !isMeaningfulFontColor(style.fontColor)
            )
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
    const Workbook& workbook,
    const QVector<MonthBlock>& blocks,
    const QHash<int, Cell>& cellsByPosition,
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

}

QString calendarEventImportSignature(
    const CalendarEvent& event
    )
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(
            event.title.simplified(),
            normalizedCalendarEventType(event.eventType),
            event.startDate.toString(Qt::ISODate),
            event.endDate.toString(Qt::ISODate),
            event.allDay ? QStringLiteral("1") : QStringLiteral("0"),
            normalizedCalendarEventTimeStatus(event.timeStatus)
            );
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

ParsedCalendarImport parseCalendarEventsFromWorkbook(
    const Workbook& workbook
    )
{
    ParsedCalendarImport result;
    const QVector<MonthBlock> blocks =
        monthBlocks(workbook);

    QHash<int, Cell> cellsByPosition;
    for (const Cell& cell : workbook.cells)
    {
        cellsByPosition.insert(
            cell.row * 100 + cell.column,
            cell
            );
    }

    QSet<QDate> cancelledDates;
    const QHash<QDate, QStringList> titlesByDate =
        noteTitlesByDate(
            workbook,
            blocks,
            cellsByPosition,
            &cancelledDates
            );
    const QHash<QString, QString> labelsByFill =
        fillLegend(workbook);
    const QHash<QString, QString> labelsByFont =
        fontLegend(workbook);
    QSet<QString> emitted;

    for (const MonthBlock& block : blocks)
    {
        for (int row = block.row + 2;
             row < block.row + 2 + CalendarGridRows;
             ++row)
        {
            for (int column = block.column;
                 column < block.column + CalendarGridColumns;
                 ++column)
            {
                const Cell cell =
                    cellsByPosition.value(row * 100 + column);

                if (cell.value.trimmed().isEmpty())
                {
                    continue;
                }

                bool dayOk = false;
                const int displayedDay =
                    cell.value.toDouble(&dayOk);

                if (!dayOk || displayedDay <= 0)
                {
                    continue;
                }

                const QDate date =
                    gridDate(block, row, column);

                if (
                    !date.isValid()
                    || date.day() != displayedDay
                    || cancelledDates.contains(date)
                    )
                {
                    continue;
                }

                const Style style =
                    cellStyle(workbook, cell);
                QStringList labels;

                const QString fill =
                    normalizedColor(style.fillColor);
                if (labelsByFill.contains(fill))
                {
                    labels.append(
                        labelsByFill.value(fill)
                        );
                }

                const QString font =
                    normalizedColor(style.fontColor);
                if (labelsByFont.contains(font))
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
                        const CalendarEvent event =
                            calendarEvent(
                                date,
                                title,
                                eventType
                                );
                        const QString signature =
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

}

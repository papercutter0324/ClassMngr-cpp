#include "schedule_view_model.h"

#include "features/schedule/ui/schedule_time_formatter.h"

#include <algorithm>

#include <QTime>

namespace
{
constexpr int RegularEarlyEmptyFinalHour = 15;

bool isRegularEarlyEmptySlot(
    const QString& timeLabel
    )
{
    const QTime time =
        QTime::fromString(
            timeLabel,
            QStringLiteral("HH:mm")
            );

    return time.isValid()
        && time.hour() <= RegularEarlyEmptyFinalHour;
}

bool rowHasVisibleContent(
    const ScheduleRow& scheduleRow,
    const ScheduleBuildResult& result,
    const ScheduleViewRequest& request
    )
{
    for (const QString& day : request.days)
    {
        const QList<ScheduleEntry> entries =
            result.schedule
                .value(day)
                .value(scheduleRow.label);

        if (!entries.isEmpty())
        {
            return true;
        }

        const QString defaultState =
            scheduleDefaultSlotState(
                day,
                scheduleRow.label,
                request.useIntensive
                );

        const QString state =
            scheduleSlotTogglingEnabled(
                day,
                request.useIntensive,
                request.regularWeekdaySlotTogglingEnabled
                )
                ? scheduleSlotState(
                    day,
                    scheduleRow.label,
                    defaultState,
                    request.slotStateOverrides
                    )
                : defaultState;

        if (state != scheduleEmptySlotState())
        {
            return true;
        }
    }

    return false;
}

QList<ScheduleRow> filteredRows(
    const ScheduleBuildResult& result,
    const ScheduleViewRequest& request
    )
{
    if (
        !request.useIntensive
        || request.rowFilter == ScheduleRowFilter::None
        )
    {
        return result.rows;
    }

    if (request.rowFilter == ScheduleRowFilter::HideEmptyRows)
    {
        QList<ScheduleRow> rows;

        for (const ScheduleRow& scheduleRow : result.rows)
        {
            if (
                rowHasVisibleContent(
                    scheduleRow,
                    result,
                    request
                    )
                )
            {
                rows.append(scheduleRow);
            }
        }

        return rows;
    }

    int firstVisibleRow = -1;
    int lastVisibleRow = -1;

    for (int rowIndex = 0; rowIndex < result.rows.size(); ++rowIndex)
    {
        if (
            rowHasVisibleContent(
                result.rows[rowIndex],
                result,
                request
                )
            )
        {
            if (firstVisibleRow < 0)
            {
                firstVisibleRow = rowIndex;
            }

            lastVisibleRow = rowIndex;
        }
    }

    return firstVisibleRow >= 0
        ? result.rows.mid(
            firstVisibleRow,
            lastVisibleRow - firstVisibleRow + 1
            )
        : QList<ScheduleRow>();
}
}

QString scheduleEmptySlotState()
{
    return QStringLiteral("empty");
}

QString scheduleEssaySlotState()
{
    return QStringLiteral("essay");
}

QString scheduleLunchSlotState()
{
    return QStringLiteral("lunch");
}

QString nextScheduleSlotState(
    const QString& currentState
    )
{
    if (currentState == scheduleEssaySlotState())
    {
        return scheduleLunchSlotState();
    }

    if (currentState == scheduleLunchSlotState())
    {
        return scheduleEmptySlotState();
    }

    return scheduleEssaySlotState();
}

QString scheduleSlotKey(
    const QString& day,
    const QString& timeLabel
    )
{
    return day + QLatin1Char('\x1f') + timeLabel;
}

QStringList visibleScheduleDays(
    bool includeWeekends
    )
{
    QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    if (includeWeekends)
    {
        days.append(
            QStringLiteral("Saturday")
            );

        days.append(
            QStringLiteral("Sunday")
            );
    }

    return days;
}

bool isScheduleWeekendDay(
    const QString& day
    )
{
    return day == QStringLiteral("Saturday")
        || day == QStringLiteral("Sunday");
}

QString scheduleDefaultSlotState(
    const QString& day,
    const QString& timeLabel,
    bool useIntensive
    )
{
    if (isScheduleWeekendDay(day))
    {
        return scheduleEmptySlotState();
    }

    if (useIntensive)
    {
        return scheduleEssaySlotState();
    }

    if (isRegularEarlyEmptySlot(timeLabel))
    {
        return scheduleEmptySlotState();
    }

    return scheduleEssaySlotState();
}

bool scheduleSlotTogglingEnabled(
    const QString& day,
    bool useIntensive,
    bool regularWeekdaySlotTogglingEnabled
    )
{
    if (useIntensive)
    {
        return true;
    }

    return isScheduleWeekendDay(day)
        || regularWeekdaySlotTogglingEnabled;
}

QString scheduleSlotState(
    const QString& day,
    const QString& timeLabel,
    const QString& defaultState,
    const QMap<QString, QString>& overrides
    )
{
    return overrides.value(
        scheduleSlotKey(
            day,
            timeLabel
            ),
        defaultState
        );
}

ScheduleViewModel buildScheduleViewModel(
    const ScheduleBuildResult& result,
    const ScheduleViewRequest& request
    )
{
    ScheduleViewModel model;
    model.days =
        request.days.isEmpty()
            ? result.days
            : request.days;
    model.uses55Endings =
        result.uses55Endings;

    ScheduleViewRequest resolvedRequest =
        request;
    resolvedRequest.days =
        model.days;

    const QList<ScheduleRow> rows =
        filteredRows(
            result,
            resolvedRequest
            );

    for (const ScheduleRow& scheduleRow : rows)
    {
        ScheduleRowView row;
        row.timeLabel =
            scheduleRow.label;
        row.timeRangeLabel =
            ScheduleTimeFormatter::rangeLabel(
                scheduleRow.label,
                result.uses55Endings,
                request.use24h
                );

        for (const QString& day : model.days)
        {
            ScheduleCellView cell;
            cell.day = day;
            cell.timeLabel = scheduleRow.label;
            cell.entries =
                result.schedule
                    .value(day)
                    .value(scheduleRow.label);
            cell.defaultSlotState =
                scheduleDefaultSlotState(
                    day,
                    scheduleRow.label,
                    request.useIntensive
                    );
            cell.slotTogglingEnabled =
                scheduleSlotTogglingEnabled(
                    day,
                    request.useIntensive,
                    request.regularWeekdaySlotTogglingEnabled
                    );
            cell.slotState =
                cell.slotTogglingEnabled
                    ? scheduleSlotState(
                        day,
                        scheduleRow.label,
                        cell.defaultSlotState,
                        request.slotStateOverrides
                        )
                    : cell.defaultSlotState;

            if (!cell.entries.isEmpty())
            {
                row.maxEntryCount =
                    std::max(
                        row.maxEntryCount,
                        static_cast<int>(cell.entries.size())
                        );
                ++model.summary.scheduledBlocks;
            }
            else if (cell.slotState == scheduleEssaySlotState())
            {
                ++model.summary.essayBlocks;
                ++model.summary.scheduledBlocks;
            }

            row.cells.append(cell);
        }

        model.rows.append(row);
    }

    return model;
}

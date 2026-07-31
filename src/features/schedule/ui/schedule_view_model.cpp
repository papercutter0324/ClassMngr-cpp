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
                scheduleModeUsesIntensiveTimes(
                    request.displayMode
                    )
                );

        const QString state =
            scheduleSlotTogglingEnabled(
                day,
                scheduleModeUsesIntensiveTimes(
                    request.displayMode
                    ),
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
        !scheduleModeUsesIntensiveTimes(
            request.displayMode
            )
        || request.rowFilter == ScheduleRowFilter::None
        )
    {
        return result.rows;
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

bool testingSuppressesEntry(
    const ScheduleEntry& entry,
    bool testingAffectsM1
    )
{
    const QString grade =
        entry.classGrade.trimmed().toUpper();

    return grade == QStringLiteral("M2")
        || grade == QStringLiteral("M3")
        || (
            testingAffectsM1
            && grade == QStringLiteral("M1")
            );
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

QString scheduleTestingSlotState()
{
    return QStringLiteral("testing");
}

bool scheduleModeUsesIntensiveTimes(
    ScheduleDisplayMode mode
    )
{
    return mode == ScheduleDisplayMode::Intensive;
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

            const QString assignmentKey =
                scheduleSlotKey(
                    day,
                    scheduleRow.label
                    );
            const auto assignment =
                request.testingAssignments.constFind(
                    assignmentKey
                    );
            const bool hasExplicitAssignment =
                request.displayMode == ScheduleDisplayMode::Testing
                && assignment != request.testingAssignments.cend();
            bool removedAffectedEntry = false;

            if (
                request.displayMode
                    == ScheduleDisplayMode::Testing
                && !hasExplicitAssignment
                )
            {
                for (
                    int entryIndex = cell.entries.size() - 1;
                    entryIndex >= 0;
                    --entryIndex
                    )
                {
                    if (
                        testingSuppressesEntry(
                            cell.entries.at(entryIndex),
                            request.testingAffectsM1
                            )
                        )
                    {
                        cell.entries.removeAt(entryIndex);
                        removedAffectedEntry = true;
                    }
                }
            }

            cell.defaultSlotState =
                scheduleDefaultSlotState(
                    day,
                    scheduleRow.label,
                    scheduleModeUsesIntensiveTimes(
                        request.displayMode
                        )
                    );
            cell.slotTogglingEnabled =
                scheduleSlotTogglingEnabled(
                    day,
                    scheduleModeUsesIntensiveTimes(
                        request.displayMode
                        ),
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

            if (hasExplicitAssignment)
            {
                cell.entries.clear();

                if (
                    assignment->assignment.kind
                        == TestingAssignmentKind::SpecialClass
                    )
                {
                    cell.entries.append(
                        assignment->testingClassEntry
                        );
                    cell.testingClassAssignment = true;
                    cell.testingClassId =
                        assignment->assignment.classId;
                }
                else
                {
                    cell.slotState =
                        scheduleTestingSlotState();
                    cell.testingRoom =
                        assignment->assignment.room;
                }
            }

            if (
                request.displayMode
                    == ScheduleDisplayMode::Testing
                && cell.entries.isEmpty()
                && !hasExplicitAssignment
                )
            {
                if (removedAffectedEntry)
                {
                    cell.slotState =
                        scheduleEssaySlotState();
                }

                cell.testingBlockCreationEnabled =
                    cell.slotState
                        == scheduleEssaySlotState();
            }

            if (!cell.entries.isEmpty())
            {
                row.maxEntryCount =
                    std::max(
                        row.maxEntryCount,
                        static_cast<int>(cell.entries.size())
                        );
                ++model.summary.scheduledBlocks;

                if (cell.testingClassAssignment)
                {
                    ++model.summary.testingClassBlocks;
                }
            }
            else if (cell.slotState == scheduleEssaySlotState())
            {
                ++model.summary.essayBlocks;
                ++model.summary.scheduledBlocks;
            }
            else if (cell.slotState == scheduleTestingSlotState())
            {
                ++model.summary.testingBlocks;
                ++model.summary.scheduledBlocks;
            }

            row.cells.append(cell);
        }

        model.rows.append(row);
    }

    return model;
}

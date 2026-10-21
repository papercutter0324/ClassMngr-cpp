#include "schedule_view_model.h"

#include "domain/rules/schedule_value_parser.h"
#include "features/schedule/ui/schedule_time_formatter.h"
#include "next/application/schedule_slot_policy.h"
#include "next/application/schedule_testing_grade_suppression_policy.h"
#include "next/domain/course.h"

#include <algorithm>
#include <optional>

namespace
{
using ClassMngr::Next::Application::ScheduleSlotPolicy;
using ClassMngr::Next::Application::ScheduleSlotCanonicalHour;
using ClassMngr::Next::Application::ScheduleSlotState;
using ClassMngr::Next::Application::scheduleSlotPolicy;
using ClassMngr::Next::Application::scheduleTestingSuppressesGradeBand;

std::optional<ScheduleSlotCanonicalHour> canonicalScheduleHour(
    const QString& timeLabel
    )
{
    const auto parsedTime =
        ScheduleValueParser::parseTime(timeLabel);

    if (!parsedTime || parsedTime->text != timeLabel)
    {
        return std::nullopt;
    }

    return ScheduleSlotCanonicalHour{parsedTime->value.hour()};
}

ScheduleSlotPolicy resolvedScheduleSlotPolicy(
    const QString& day,
    const QString& timeLabel,
    bool useIntensive,
    bool regularWeekdaySlotTogglingEnabled
    )
{
    return scheduleSlotPolicy(
        isScheduleWeekendDay(day),
        canonicalScheduleHour(timeLabel),
        useIntensive,
        regularWeekdaySlotTogglingEnabled
        );
}

QString scheduleSlotStateText(
    ScheduleSlotState state
    )
{
    return state == ScheduleSlotState::Empty
        ? scheduleEmptySlotState()
        : scheduleEssaySlotState();
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

        const ScheduleSlotPolicy policy =
            resolvedScheduleSlotPolicy(
                day,
                scheduleRow.label,
                scheduleModeUsesIntensiveTimes(
                    request.displayMode
                    ),
                request.regularWeekdaySlotTogglingEnabled
                );
        const QString defaultState =
            scheduleSlotStateText(policy.defaultState);

        const QString state =
            policy.togglingEnabled
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
    const auto gradeBand =
        ClassMngr::Next::Domain::Course::gradeBandForName(
            entry.classGrade.trimmed().toUpper().toStdString()
            );

    return scheduleTestingSuppressesGradeBand(
        gradeBand,
        testingAffectsM1
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
    const auto parsed = ScheduleValueParser::parseWeekday(day);
    return parsed
        && parsed->text == day
        && (parsed->value == ScheduleValueParser::Weekday::Saturday
            || parsed->value == ScheduleValueParser::Weekday::Sunday);
}

QString scheduleDefaultSlotState(
    const QString& day,
    const QString& timeLabel,
    bool useIntensive
    )
{
    return scheduleSlotStateText(
        resolvedScheduleSlotPolicy(
            day,
            timeLabel,
            useIntensive,
            false
            ).defaultState
        );
}

bool scheduleSlotTogglingEnabled(
    const QString& day,
    bool useIntensive,
    bool regularWeekdaySlotTogglingEnabled
    )
{
    return resolvedScheduleSlotPolicy(
        day,
        QString(),
        useIntensive,
        regularWeekdaySlotTogglingEnabled
        ).togglingEnabled;
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

            const ScheduleSlotPolicy policy =
                resolvedScheduleSlotPolicy(
                    day,
                    scheduleRow.label,
                    scheduleModeUsesIntensiveTimes(
                        request.displayMode
                        ),
                    request.regularWeekdaySlotTogglingEnabled
                    );
            cell.defaultSlotState =
                scheduleSlotStateText(policy.defaultState);
            cell.slotTogglingEnabled = policy.togglingEnabled;
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

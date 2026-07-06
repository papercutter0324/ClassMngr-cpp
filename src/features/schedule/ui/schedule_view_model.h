#pragma once

#include "features/schedule/ui/schedule_builder.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

enum class ScheduleRowFilter
{
    None,
    HideEmptyRows,
    TrimEmptyOuterRows
};

struct ScheduleViewRequest
{
    QStringList days;
    QMap<QString, QString> slotStateOverrides;
    bool use24h = false;
    bool useIntensive = false;
    bool regularWeekdaySlotTogglingEnabled = false;
    ScheduleRowFilter rowFilter = ScheduleRowFilter::None;
};

struct ScheduleCellView
{
    QString day;
    QString timeLabel;
    QList<ScheduleEntry> entries;
    QString defaultSlotState;
    QString slotState;
    bool slotTogglingEnabled = false;
};

struct ScheduleRowView
{
    QString timeLabel;
    QString timeRangeLabel;
    QList<ScheduleCellView> cells;
    int maxEntryCount = 1;
};

struct ScheduleSummary
{
    int essayBlocks = 0;
    int scheduledBlocks = 0;
};

struct ScheduleViewModel
{
    QStringList days;
    QList<ScheduleRowView> rows;
    bool uses55Endings = false;
    ScheduleSummary summary;
};

QString scheduleEmptySlotState();
QString scheduleEssaySlotState();
QString scheduleLunchSlotState();

QString nextScheduleSlotState(
    const QString& currentState
    );

QString scheduleSlotKey(
    const QString& day,
    const QString& timeLabel
    );

QStringList visibleScheduleDays(
    bool includeWeekends
    );

bool isScheduleWeekendDay(
    const QString& day
    );

QString scheduleDefaultSlotState(
    const QString& day,
    const QString& timeLabel,
    bool useIntensive
    );

bool scheduleSlotTogglingEnabled(
    const QString& day,
    bool useIntensive,
    bool regularWeekdaySlotTogglingEnabled
    );

QString scheduleSlotState(
    const QString& day,
    const QString& timeLabel,
    const QString& defaultState,
    const QMap<QString, QString>& overrides
    );

ScheduleViewModel buildScheduleViewModel(
    const ScheduleBuildResult& result,
    const ScheduleViewRequest& request
    );

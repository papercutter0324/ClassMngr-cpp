#pragma once

#include "domain/models/testing_block.h"
#include "features/schedule/ui/schedule_builder.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

enum class ScheduleRowFilter
{
    None,
    TrimEmptyOuterRows
};

enum class ScheduleDisplayMode
{
    Regular,
    Intensive,
    Testing
};

struct TestingAssignmentView
{
    TestingAssignment assignment;
    ScheduleEntry testingClassEntry;
};

struct ScheduleViewRequest
{
    QStringList days;
    QMap<QString, QString> slotStateOverrides;
    QMap<QString, TestingAssignmentView> testingAssignments;
    bool use24h = false;
    ScheduleDisplayMode displayMode =
        ScheduleDisplayMode::Regular;
    bool testingAffectsM1 = false;
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
    QString testingRoom;
    bool testingClassAssignment = false;
    int testingClassId{-1};
    bool slotTogglingEnabled = false;
    bool testingBlockCreationEnabled = false;
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
    int testingBlocks = 0;
    int testingClassBlocks = 0;
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
QString scheduleTestingSlotState();

bool scheduleModeUsesIntensiveTimes(
    ScheduleDisplayMode mode
    );

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

inline QString scheduleTeacherName(
    const ScheduleEntry& entry,
    bool showEnglishName
    )
{
    const QString preferredName =
        (showEnglishName
            ? entry.teacherPreferredName
            : entry.teacherKr)
            .trimmed();
    const QString fallbackName =
        (showEnglishName
            ? entry.teacherEn
            : entry.teacherEn)
            .trimmed();

    return preferredName.isEmpty()
        ? (fallbackName.isEmpty()
            ? entry.teacherKr.trimmed()
            : fallbackName)
        : preferredName;
}

inline QString scheduleTeacherRoomLine(
    const ScheduleEntry& entry,
    bool showEnglishName
    )
{
    return QStringLiteral("%1 %2")
        .arg(
            scheduleTeacherName(
                entry,
                showEnglishName
                ),
            entry.roomNumber.trimmed()
            )
        .simplified();
}

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

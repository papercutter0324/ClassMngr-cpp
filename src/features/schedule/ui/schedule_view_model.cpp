#include "schedule_view_model.h"

#include "classmngr/engine/schedule_report.h"

#include <utility>

namespace
{
using classmngr::engine::ScheduleReportBuildResult;
using classmngr::engine::ScheduleReportCell;
using classmngr::engine::ScheduleReportDisplayMode;
using classmngr::engine::ScheduleReportEntry;
using classmngr::engine::ScheduleReportEntryKind;
using classmngr::engine::ScheduleReportModel;
using classmngr::engine::ScheduleReportRequest;
using classmngr::engine::ScheduleReportRow;
using classmngr::engine::ScheduleReportRowFilter;
using classmngr::engine::ScheduleReportService;
using classmngr::engine::ScheduleReportTestingAssignmentKind;
using classmngr::engine::ScheduleReportTestingAssignmentView;

std::string toPortable(const QString& value)
{
    return value.toStdString();
}

QString fromPortable(const std::string& value)
{
    return QString::fromStdString(value);
}

ScheduleReportEntry toPortable(const ScheduleEntry& entry)
{
    ScheduleReportEntry result;
    result.classId = entry.classId;
    result.kind = entry.kind == ScheduleEntryKind::TestingClass
        ? ScheduleReportEntryKind::TestingClass
        : ScheduleReportEntryKind::RegularClass;
    result.className = toPortable(entry.className);
    result.teacherKr = toPortable(entry.teacherKr);
    result.teacherEn = toPortable(entry.teacherEn);
    result.teacherPreferredName = toPortable(entry.teacherPreferredName);
    result.roomNumber = toPortable(entry.roomNumber);
    result.classGrade = toPortable(entry.classGrade);
    result.classLevel = toPortable(entry.classLevel);
    result.classColor = toPortable(entry.classColor);
    result.fontColor = toPortable(entry.fontColor);
    return result;
}

ScheduleEntry fromPortable(const ScheduleReportEntry& entry)
{
    ScheduleEntry result;
    result.classId = entry.classId;
    result.kind = entry.kind == ScheduleReportEntryKind::TestingClass
        ? ScheduleEntryKind::TestingClass
        : ScheduleEntryKind::RegularClass;
    result.className = fromPortable(entry.className);
    result.teacherKr = fromPortable(entry.teacherKr);
    result.teacherEn = fromPortable(entry.teacherEn);
    result.teacherPreferredName = fromPortable(entry.teacherPreferredName);
    result.roomNumber = fromPortable(entry.roomNumber);
    result.classGrade = fromPortable(entry.classGrade);
    result.classLevel = fromPortable(entry.classLevel);
    result.classColor = fromPortable(entry.classColor);
    result.fontColor = fromPortable(entry.fontColor);
    return result;
}

ScheduleReportBuildResult toPortable(const ScheduleBuildResult& result)
{
    ScheduleReportBuildResult portable;
    portable.days.reserve(result.days.size());
    for (const QString& day : result.days)
    {
        portable.days.push_back(toPortable(day));
    }

    portable.rows.reserve(result.rows.size());
    for (const ScheduleRow& row : result.rows)
    {
        portable.rows.push_back({toPortable(row.label)});
    }

    for (auto dayIterator = result.schedule.cbegin();
         dayIterator != result.schedule.cend();
         ++dayIterator)
    {
        auto& portableSlots =
            portable.schedule[toPortable(dayIterator.key())];
        for (auto slotIterator = dayIterator.value().cbegin();
             slotIterator != dayIterator.value().cend();
             ++slotIterator)
        {
            auto& entries =
                portableSlots[toPortable(slotIterator.key())];
            entries.reserve(slotIterator.value().size());
            for (const ScheduleEntry& entry : slotIterator.value())
            {
                entries.push_back(toPortable(entry));
            }
        }
    }

    portable.scheduleOffset = result.scheduleOffset;
    portable.uses55Endings = result.uses55Endings;
    return portable;
}

ScheduleReportDisplayMode toPortable(ScheduleDisplayMode mode)
{
    switch (mode)
    {
    case ScheduleDisplayMode::Intensive:
        return ScheduleReportDisplayMode::Intensive;
    case ScheduleDisplayMode::Testing:
        return ScheduleReportDisplayMode::Testing;
    case ScheduleDisplayMode::Regular:
    default:
        return ScheduleReportDisplayMode::Regular;
    }
}

ScheduleReportRowFilter toPortable(ScheduleRowFilter filter)
{
    return filter == ScheduleRowFilter::TrimEmptyOuterRows
        ? ScheduleReportRowFilter::TrimEmptyOuterRows
        : ScheduleReportRowFilter::None;
}

ScheduleReportTestingAssignmentKind toPortable(TestingAssignmentKind kind)
{
    return kind == TestingAssignmentKind::SpecialClass
        ? ScheduleReportTestingAssignmentKind::SpecialClass
        : ScheduleReportTestingAssignmentKind::PlainTesting;
}

ScheduleReportRequest toPortable(const ScheduleViewRequest& request)
{
    ScheduleReportRequest portable;
    portable.days.reserve(request.days.size());
    for (const QString& day : request.days)
    {
        portable.days.push_back(toPortable(day));
    }

    for (auto iterator = request.slotStateOverrides.cbegin();
         iterator != request.slotStateOverrides.cend();
         ++iterator)
    {
        portable.slotStateOverrides.emplace(
            toPortable(iterator.key()),
            toPortable(iterator.value())
            );
    }

    for (auto iterator = request.testingAssignments.cbegin();
         iterator != request.testingAssignments.cend();
         ++iterator)
    {
        ScheduleReportTestingAssignmentView assignment;
        assignment.assignment.day = toPortable(iterator.value().assignment.day);
        assignment.assignment.startTime =
            toPortable(iterator.value().assignment.startTime);
        assignment.assignment.kind = toPortable(
            iterator.value().assignment.kind
            );
        assignment.assignment.room = toPortable(
            iterator.value().assignment.room
            );
        assignment.assignment.classId = iterator.value().assignment.classId;
        assignment.testingClassEntry = toPortable(
            iterator.value().testingClassEntry
            );
        portable.testingAssignments.emplace(
            toPortable(iterator.key()),
            std::move(assignment)
            );
    }

    portable.use24h = request.use24h;
    portable.displayMode = toPortable(request.displayMode);
    portable.testingAffectsM1 = request.testingAffectsM1;
    portable.regularWeekdaySlotTogglingEnabled =
        request.regularWeekdaySlotTogglingEnabled;
    portable.rowFilter = toPortable(request.rowFilter);
    return portable;
}

ScheduleCellView fromPortable(const ScheduleReportCell& cell)
{
    ScheduleCellView result;
    result.day = fromPortable(cell.day);
    result.timeLabel = fromPortable(cell.timeLabel);
    result.defaultSlotState = fromPortable(cell.defaultSlotState);
    result.slotState = fromPortable(cell.slotState);
    result.testingRoom = fromPortable(cell.testingRoom);
    result.testingClassAssignment = cell.testingClassAssignment;
    result.testingClassId = cell.testingClassId;
    result.slotTogglingEnabled = cell.slotTogglingEnabled;
    result.testingBlockCreationEnabled = cell.testingBlockCreationEnabled;
    result.entries.reserve(static_cast<qsizetype>(cell.entries.size()));
    for (const ScheduleReportEntry& entry : cell.entries)
    {
        result.entries.append(fromPortable(entry));
    }
    return result;
}

ScheduleViewModel fromPortable(const ScheduleReportModel& model)
{
    ScheduleViewModel result;
    result.uses55Endings = model.uses55Endings;
    result.days.reserve(static_cast<qsizetype>(model.days.size()));
    for (const std::string& day : model.days)
    {
        result.days.append(fromPortable(day));
    }

    result.rows.reserve(static_cast<qsizetype>(model.rows.size()));
    for (const auto& sourceRow : model.rows)
    {
        ScheduleRowView row;
        row.timeLabel = fromPortable(sourceRow.timeLabel);
        row.timeRangeLabel = fromPortable(sourceRow.timeRangeLabel);
        row.maxEntryCount = sourceRow.maxEntryCount;
        row.cells.reserve(static_cast<qsizetype>(sourceRow.cells.size()));
        for (const ScheduleReportCell& cell : sourceRow.cells)
        {
            row.cells.append(fromPortable(cell));
        }
        result.rows.append(std::move(row));
    }

    result.summary.essayBlocks = model.summary.essayBlocks;
    result.summary.testingBlocks = model.summary.testingBlocks;
    result.summary.testingClassBlocks = model.summary.testingClassBlocks;
    result.summary.scheduledBlocks = model.summary.scheduledBlocks;
    return result;
}
} // namespace

QString scheduleEmptySlotState()
{
    return fromPortable(ScheduleReportService::emptySlotState());
}

QString scheduleEssaySlotState()
{
    return fromPortable(ScheduleReportService::essaySlotState());
}

QString scheduleLunchSlotState()
{
    return fromPortable(ScheduleReportService::lunchSlotState());
}

QString scheduleTestingSlotState()
{
    return fromPortable(ScheduleReportService::testingSlotState());
}

bool scheduleModeUsesIntensiveTimes(ScheduleDisplayMode mode)
{
    return ScheduleReportService::modeUsesIntensiveTimes(toPortable(mode));
}

QString nextScheduleSlotState(const QString& currentState)
{
    return fromPortable(
        ScheduleReportService::nextSlotState(toPortable(currentState))
        );
}

QString scheduleSlotKey(
    const QString& day,
    const QString& timeLabel
    )
{
    return fromPortable(
        ScheduleReportService::slotKey(
            toPortable(day),
            toPortable(timeLabel)
            )
        );
}

QStringList visibleScheduleDays(bool includeWeekends)
{
    QStringList result;
    const std::vector<std::string> days =
        ScheduleReportService::visibleDays(includeWeekends);
    result.reserve(static_cast<qsizetype>(days.size()));
    for (const std::string& day : days)
    {
        result.append(fromPortable(day));
    }
    return result;
}

bool isScheduleWeekendDay(const QString& day)
{
    return ScheduleReportService::isWeekendDay(toPortable(day));
}

QString scheduleDefaultSlotState(
    const QString& day,
    const QString& timeLabel,
    bool useIntensive
    )
{
    return fromPortable(
        ScheduleReportService::defaultSlotState(
            toPortable(day),
            toPortable(timeLabel),
            useIntensive
            )
        );
}

bool scheduleSlotTogglingEnabled(
    const QString& day,
    bool useIntensive,
    bool regularWeekdaySlotTogglingEnabled
    )
{
    return ScheduleReportService::slotTogglingEnabled(
        toPortable(day),
        useIntensive,
        regularWeekdaySlotTogglingEnabled
        );
}

QString scheduleTeacherName(
    const ScheduleEntry& entry,
    bool showEnglishName
    )
{
    return fromPortable(
        ScheduleReportService::teacherName(
            toPortable(entry),
            showEnglishName
            )
        );
}

QString scheduleTeacherRoomLine(
    const ScheduleEntry& entry,
    bool showEnglishName
    )
{
    return fromPortable(
        ScheduleReportService::teacherRoomLine(
            toPortable(entry),
            showEnglishName
            )
        );
}

QString scheduleSlotState(
    const QString& day,
    const QString& timeLabel,
    const QString& defaultState,
    const QMap<QString, QString>& overrides
    )
{
    std::map<std::string, std::string> portableOverrides;
    for (auto iterator = overrides.cbegin();
         iterator != overrides.cend();
         ++iterator)
    {
        portableOverrides.emplace(
            toPortable(iterator.key()),
            toPortable(iterator.value())
            );
    }

    return fromPortable(
        ScheduleReportService::slotState(
            toPortable(day),
            toPortable(timeLabel),
            toPortable(defaultState),
            portableOverrides
            )
        );
}

ScheduleViewModel buildScheduleViewModel(
    const ScheduleBuildResult& result,
    const ScheduleViewRequest& request
    )
{
    return fromPortable(
        ScheduleReportService::build(
            toPortable(result),
            toPortable(request)
            )
        );
}

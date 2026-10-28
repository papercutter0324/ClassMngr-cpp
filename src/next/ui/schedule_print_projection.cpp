#include "next/ui/schedule_print_projection.h"

#include "next/ui/schedule_page_model.h"

#include <QTime>

namespace ClassMngr::Next::Ui
{
namespace
{

[[nodiscard]] SchedulePrintService::SlotState printSlotState(
    const Application::ScheduleSlotState state
    ) noexcept
{
    switch (state)
    {
    case Application::ScheduleSlotState::Empty:
        return SchedulePrintService::SlotState::Empty;
    case Application::ScheduleSlotState::Essay:
        return SchedulePrintService::SlotState::Essay;
    case Application::ScheduleSlotState::Lunch:
        return SchedulePrintService::SlotState::Lunch;
    }
    return SchedulePrintService::SlotState::Empty;
}

[[nodiscard]] QString canonicalTimeLabel(const int startMinute)
{
    const QTime time(startMinute / 60, startMinute % 60);
    return time.isValid() ? time.toString(QStringLiteral("HH:mm")) : QString{};
}

} // namespace

SchedulePrintService::Document schedulePrintDocument(
    const SchedulePageModel& model
    )
{
    using namespace SchedulePrintService;
    Document document;
    document.days = model.days();

    document.rows.reserve(model.rowCount());
    for (int rowIndex = 0; rowIndex < model.rowCount(); ++rowIndex)
    {
        const SchedulePageRow* const sourceRow = model.rowAt(rowIndex);
        if (!sourceRow)
        {
            continue;
        }
        const QString timeLabel = canonicalTimeLabel(sourceRow->startMinute);
        Row row;
        row.timeLabel = timeLabel;
        row.timeRangeLabel = sourceRow->timeRangeLabel;
        row.cells.reserve(static_cast<qsizetype>(sourceRow->cells.size()));
        for (const SchedulePageCell& sourceCell : sourceRow->cells)
        {
            Cell cell;
            cell.day = sourceCell.day;
            cell.timeLabel = timeLabel;
            cell.slotState = sourceCell.plainTestingAssignment
                ? SlotState::Testing : printSlotState(sourceCell.state);
            cell.testingRoom = sourceCell.testingRoom;
            cell.entries.reserve(static_cast<qsizetype>(sourceCell.entries.size()));
            for (const SchedulePageEntry& sourceEntry : sourceCell.entries)
            {
                cell.entries.append({
                    .className = sourceEntry.className,
                    .teacherKoreanName = sourceEntry.teacherKoreanName,
                    .teacherEnglishName = sourceEntry.teacherEnglishName,
                    .teacherPreferredName = sourceEntry.teacherPreferredName,
                    .room = sourceEntry.room,
                    .grade = sourceEntry.grade,
                    .level = sourceEntry.level,
                    .classColor = sourceEntry.classColor,
                    .fontColor = sourceEntry.fontColor,
                    .testingClass = sourceEntry.testingClass
                });
            }

            if (!cell.entries.isEmpty())
            {
                ++document.summary.scheduledBlocks;
            }
            else if (cell.slotState == SlotState::Testing)
            {
                ++document.summary.scheduledBlocks;
            }
            else if (cell.slotState == SlotState::Essay)
            {
                ++document.summary.essayBlocks;
                ++document.summary.scheduledBlocks;
            }
            row.cells.append(std::move(cell));
        }
        document.rows.append(std::move(row));
    }
    return document;
}

} // namespace ClassMngr::Next::Ui

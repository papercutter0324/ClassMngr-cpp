#include "features/schedule/ui/schedule_print_document_adapter.h"

#include <utility>

SchedulePrintService::Document schedulePrintDocumentFromLegacyModel(
    const ScheduleViewModel& model
    )
{
    using namespace SchedulePrintService;
    Document document;
    document.days = model.days;
    document.summary = {
        .essayBlocks = model.summary.essayBlocks,
        .scheduledBlocks = model.summary.scheduledBlocks
    };
    document.rows.reserve(model.rows.size());
    for (const ScheduleRowView& sourceRow : model.rows)
    {
        Row row;
        row.timeLabel = sourceRow.timeLabel;
        row.timeRangeLabel = sourceRow.timeRangeLabel;
        row.cells.reserve(sourceRow.cells.size());
        for (const ScheduleCellView& sourceCell : sourceRow.cells)
        {
            Cell cell;
            cell.day = sourceCell.day;
            cell.timeLabel = sourceCell.timeLabel;
            cell.testingRoom = sourceCell.testingRoom;
            if (sourceCell.slotState == QStringLiteral("essay"))
            {
                cell.slotState = SlotState::Essay;
            }
            else if (sourceCell.slotState == QStringLiteral("lunch"))
            {
                cell.slotState = SlotState::Lunch;
            }
            else if (sourceCell.slotState == QStringLiteral("testing"))
            {
                cell.slotState = SlotState::Testing;
            }
            else
            {
                cell.slotState = SlotState::Empty;
            }

            cell.entries.reserve(sourceCell.entries.size());
            for (const ScheduleEntry& sourceEntry : sourceCell.entries)
            {
                cell.entries.append({
                    .className = sourceEntry.className,
                    .teacherKoreanName = sourceEntry.teacherKr,
                    .teacherEnglishName = sourceEntry.teacherEn,
                    .teacherPreferredName = sourceEntry.teacherPreferredName,
                    .room = sourceEntry.roomNumber,
                    .grade = sourceEntry.classGrade,
                    .level = sourceEntry.classLevel,
                    .classColor = sourceEntry.classColor,
                    .fontColor = sourceEntry.fontColor,
                    .testingClass = sourceEntry.kind
                        == ScheduleEntryKind::TestingClass
                });
            }
            row.cells.append(std::move(cell));
        }
        document.rows.append(std::move(row));
    }
    return document;
}

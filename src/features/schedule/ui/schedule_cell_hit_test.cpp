#include "schedule_cell_hit_test.h"

#include "features/schedule/ui/schedule_view_model.h"

ScheduleCellHit ScheduleCellHitTest::hit(const ScheduleCellView* cell)
{
    ScheduleCellHit result;
    if (!cell)
    {
        return result;
    }

    result.day = cell->day;
    result.timeLabel = cell->timeLabel;
    if (cell->entries.isEmpty())
    {
        result.currentState = cell->slotState;
        result.defaultState =
            cell->defaultSlotState;
        if (
            result.currentState == scheduleTestingSlotState()
            || cell->testingBlockCreationEnabled
            )
        {
            result.command = ScheduleCellCommand::EditTestingAssignment;
        }
        else if (cell->slotTogglingEnabled)
        {
            result.command = ScheduleCellCommand::ToggleSlot;
            result.nextState = nextScheduleSlotState(result.currentState);
        }
        return result;
    }

    result.classId = cell->entries.first().classId;
    if (result.classId <= 0)
    {
        return result;
    }
    result.command = (cell->entries.size() == 1 && cell->entries.first().kind == ScheduleEntryKind::TestingClass)
        ? ScheduleCellCommand::EditTestingAssignment
        : ScheduleCellCommand::EditClass;
    return result;
}

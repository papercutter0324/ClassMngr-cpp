#include "schedule_cell_hit_test.h"

#include "features/schedule/ui/schedule_view_model.h"

#include <QWidget>

ScheduleCellHit ScheduleCellHitTest::hit(const QWidget* widget)
{
    ScheduleCellHit result;
    if (!widget)
    {
        return result;
    }

    result.day = widget->property("day").toString();
    result.timeLabel = widget->property("time_label").toString();
    if (widget->property("is_slot_cell").toBool())
    {
        result.currentState = widget->property("slot_state").toString();
        result.defaultState =
            widget->property("default_slot_state").toString();
        if (
            result.currentState == scheduleTestingSlotState()
            || widget->property("testing_block_creation_enabled").toBool()
            )
        {
            result.command = ScheduleCellCommand::EditTestingAssignment;
        }
        else if (widget->property("slot_toggling_enabled").toBool())
        {
            result.command = ScheduleCellCommand::ToggleSlot;
            result.nextState = nextScheduleSlotState(result.currentState);
        }
        return result;
    }

    result.classId = widget->property("class_id").toInt();
    if (result.classId <= 0)
    {
        return result;
    }
    result.command = widget->property("testing_class_assignment").toBool()
        ? ScheduleCellCommand::EditTestingAssignment
        : ScheduleCellCommand::EditClass;
    return result;
}

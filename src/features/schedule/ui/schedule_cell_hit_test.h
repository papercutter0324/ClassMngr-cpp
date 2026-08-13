#pragma once

#include <QString>

class QWidget;

enum class ScheduleCellCommand
{
    None,
    EditClass,
    EditTestingAssignment,
    ToggleSlot
};

struct ScheduleCellHit
{
    ScheduleCellCommand command = ScheduleCellCommand::None;
    int classId = -1;
    QString day;
    QString timeLabel;
    QString currentState;
    QString nextState;
    QString defaultState;
};

class ScheduleCellHitTest final
{
public:
    [[nodiscard]] static ScheduleCellHit hit(const QWidget* widget);
};

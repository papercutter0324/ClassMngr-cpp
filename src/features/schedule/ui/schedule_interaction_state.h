#pragma once

#include "domain/models/intensive_slot_state.h"
#include "features/schedule/ui/schedule_view_model.h"

#include <QList>
#include <QMap>

class ScheduleInteractionState final
{
public:
    void clear();
    void clearTestingAssignments();
    void setSlotStates(const QList<IntensiveSlotState>& states);
    void setTestingAssignments(
        QMap<QString, TestingAssignmentView> assignments
        );

    [[nodiscard]] const TestingAssignmentView* testingAssignment(
        const QString& day,
        const QString& timeLabel
        ) const;
    [[nodiscard]] const QMap<QString, QString>& slotStates() const;
    [[nodiscard]] const QMap<QString, TestingAssignmentView>&
        testingAssignments() const;

private:
    QMap<QString, QString> m_slotStates;
    QMap<QString, TestingAssignmentView> m_testingAssignments;
};

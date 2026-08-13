#include "schedule_interaction_state.h"

void ScheduleInteractionState::clear()
{
    m_slotStates.clear();
    m_testingAssignments.clear();
}

void ScheduleInteractionState::clearTestingAssignments()
{
    m_testingAssignments.clear();
}

void ScheduleInteractionState::setSlotStates(
    const QList<IntensiveSlotState>& states
    )
{
    m_slotStates.clear();
    for (const IntensiveSlotState& state : states)
    {
        m_slotStates.insert(
            scheduleSlotKey(state.day, state.startTime),
            state.state
            );
    }
}

void ScheduleInteractionState::setTestingAssignments(
    QMap<QString, TestingAssignmentView> assignments
    )
{
    m_testingAssignments = std::move(assignments);
}

const TestingAssignmentView* ScheduleInteractionState::testingAssignment(
    const QString& day,
    const QString& timeLabel
    ) const
{
    const auto iterator = m_testingAssignments.constFind(
        scheduleSlotKey(day, timeLabel)
        );
    return iterator == m_testingAssignments.cend()
        ? nullptr
        : &iterator.value();
}

const QMap<QString, QString>& ScheduleInteractionState::slotStates() const
{
    return m_slotStates;
}

const QMap<QString, TestingAssignmentView>&
ScheduleInteractionState::testingAssignments() const
{
    return m_testingAssignments;
}

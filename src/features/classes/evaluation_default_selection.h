#pragma once

#include "domain/models/speaking_evaluation.h"
#include "features/calendar/academic_calendar_schedule.h"

#include <QDate>
#include <QString>

class ApplicationServices;

namespace EvaluationDefaultSelection
{

[[nodiscard]] QString evaluationNameForTerm(AcademicTerm term);

[[nodiscard]] bool isPopulated(const SpeakingEvalRows& rows);

// Returns an empty name when there is no saved term schedule for the date.
[[nodiscard]] QString forTermSchedule(
    const AcademicCalendarSchedule& schedule,
    SchoolLevel schoolLevel,
    const QDate& date,
    bool currentTermEvaluationIsPopulated
    );

// Returns an empty name when the user prefers All, the required services are
// unavailable, or there is no saved term schedule that covers today.
[[nodiscard]] QString forClass(
    ApplicationServices* services,
    int classId,
    const QDate& date = QDate::currentDate()
    );

} // namespace EvaluationDefaultSelection

#include "features/classes/evaluation_default_selection.h"

#include "next/domain/course.h"

#include <algorithm>
#include <ranges>

#include <string>

namespace
{
AcademicTerm previousTerm(AcademicTerm term)
{
    switch (term)
    {
    case AcademicTerm::Winter: return AcademicTerm::Fall;
    case AcademicTerm::Spring: return AcademicTerm::Winter;
    case AcademicTerm::Summer: return AcademicTerm::Spring;
    case AcademicTerm::Fall: return AcademicTerm::Summer;
    }

    return AcademicTerm::Winter;
}

} // namespace

namespace EvaluationDefaultSelection::Private
{

SchoolLevel schoolLevelForClassGrade(const QString& grade)
{
    const std::string normalizedGrade =
        grade.trimmed().toUpper().toStdString();

    switch (
        ClassMngr::Next::Domain::Course::gradeBandForName(
            normalizedGrade
            )
        )
    {
    case ClassMngr::Next::Domain::CourseGradeBand::M1:
    case ClassMngr::Next::Domain::CourseGradeBand::M2:
    case ClassMngr::Next::Domain::CourseGradeBand::M3:
        return SchoolLevel::Middle;

    case ClassMngr::Next::Domain::CourseGradeBand::E4:
    case ClassMngr::Next::Domain::CourseGradeBand::E5:
    case ClassMngr::Next::Domain::CourseGradeBand::E6:
    case ClassMngr::Next::Domain::CourseGradeBand::Other:
        return SchoolLevel::Elementary;
    }

    return SchoolLevel::Elementary;
}

} // namespace EvaluationDefaultSelection::Private

namespace EvaluationDefaultSelection
{

QString evaluationNameForTerm(AcademicTerm term)
{
    switch (term)
    {
    case AcademicTerm::Winter: return QStringLiteral("Winter");
    case AcademicTerm::Spring: return QStringLiteral("Speech Contest");
    case AcademicTerm::Summer: return QStringLiteral("Summer");
    case AcademicTerm::Fall: return QStringLiteral("Fall");
    }

    return {};
}

bool isPopulated(const SpeakingEvalRows& rows)
{
    return std::ranges::any_of(
        rows,
        [](const QStringList& row)
        {
            return std::ranges::any_of(
                row,
                [](const QString& value)
                {
                    return !value.trimmed().isEmpty();
                }
                );
        }
        );
}

QString forTermSchedule(
    const AcademicCalendarSchedule& schedule,
    SchoolLevel schoolLevel,
    const QDate& date,
    bool currentTermEvaluationIsPopulated
    )
{
    if (!schedule.hasSavedSchedules())
    {
        return {};
    }

    const AcademicTermPosition position = schedule.termAt(schoolLevel, date);
    if (!position.valid)
    {
        return {};
    }

    return evaluationNameForTerm(
        currentTermEvaluationIsPopulated
            ? position.term
            : previousTerm(position.term)
        );
}

} // namespace EvaluationDefaultSelection

#include "features/classes/evaluation_default_selection.h"

#include "next/application/evaluation_default_selection.h"
#include "next/domain/course.h"

#include <algorithm>
#include <optional>
#include <ranges>

#include <string>

namespace
{
using ClassMngr::Next::Application::EvaluationPeriod;

std::optional<EvaluationPeriod> evaluationPeriodFor(AcademicTerm term)
{
    switch (term)
    {
    case AcademicTerm::Winter: return EvaluationPeriod::Winter;
    case AcademicTerm::Spring: return EvaluationPeriod::Spring;
    case AcademicTerm::Summer: return EvaluationPeriod::Summer;
    case AcademicTerm::Fall: return EvaluationPeriod::Fall;
    }

    return std::nullopt;
}

QString evaluationNameForPeriod(EvaluationPeriod period)
{
    switch (period)
    {
    case EvaluationPeriod::Winter: return QStringLiteral("Winter");
    case EvaluationPeriod::Spring: return QStringLiteral("Speech Contest");
    case EvaluationPeriod::Summer: return QStringLiteral("Summer");
    case EvaluationPeriod::Fall: return QStringLiteral("Fall");
    }

    return {};
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
    const auto period = evaluationPeriodFor(term);
    return period ? evaluationNameForPeriod(*period) : QString{};
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

    const auto currentPeriod = evaluationPeriodFor(position.term);
    if (!currentPeriod)
    {
        return {};
    }

    const auto selectedPeriod =
        ClassMngr::Next::Application::selectDefaultEvaluationPeriod(
            ClassMngr::Next::Application::EvaluationDefaultPolicy::
                CurrentOrPreviousTerm,
            *currentPeriod,
            currentTermEvaluationIsPopulated
            );
    return selectedPeriod ? evaluationNameForPeriod(*selectedPeriod)
                          : QString{};
}

} // namespace EvaluationDefaultSelection

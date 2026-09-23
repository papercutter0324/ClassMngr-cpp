#include "features/classes/evaluation_default_selection.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/class_info.h"
#include "features/calendar/ui/academic_calendar_provider.h"
#include "next/platform/application_services_evaluation_default_policy_port.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"
#include "next/platform/application_services_calendar_first_day_of_week_preferences_port.h"

#include <memory>
#include <utility>

namespace
{

bool isMiddleSchoolGrade(const QString& grade)
{
    const QString normalized = grade.trimmed().toUpper();
    return normalized == QStringLiteral("M1")
        || normalized == QStringLiteral("M2")
        || normalized == QStringLiteral("M3");
}

} // namespace

namespace EvaluationDefaultSelection
{

QString forClass(
    ApplicationServices* services,
    int classId,
    const QDate& date
    )
{
    if (!services || classId <= 0)
    {
        return {};
    }

    const ClassMngr::Next::Platform::
        ApplicationServicesEvaluationDefaultPolicyPort policyPort(*services);
    if (
        policyPort.load()
        != ClassMngr::Next::Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm
        )
    {
        return {};
    }

    ClassService* classService = services->classService();
    SpeakingEvaluationService* evaluationService =
        services->speakingEvaluationService();
    if (
        !classService
        || !classService->isAvailable()
        || !evaluationService
        || !evaluationService->isAvailable()
        )
    {
        return {};
    }

    const SchoolLevel schoolLevel = isMiddleSchoolGrade(
        classService->classInfo(classId)
            .value_or(ClassInfo{})
            .classGrade
        )
        ? SchoolLevel::Middle
        : SchoolLevel::Elementary;
    auto schedulePreferences = std::make_unique<
        ClassMngr::Next::Platform::
            ApplicationServicesAcademicCalendarSchedulePreferencesPort
        >(*services);
    auto firstDayOfWeekPreferences = std::make_unique<
        ClassMngr::Next::Platform::
            ApplicationServicesCalendarFirstDayOfWeekPreferencesPort
        >(*services);
    AcademicCalendarProvider calendar(
        std::move(schedulePreferences),
        std::move(firstDayOfWeekPreferences)
        );
    if (!calendar.schedule().hasSavedSchedules())
    {
        return {};
    }

    const AcademicTermPosition position =
        calendar.schedule().termAt(schoolLevel, date);
    if (!position.valid)
    {
        return {};
    }

    const QString currentEvaluation = evaluationNameForTerm(position.term);
    const Result<SpeakingEvalRows> rows = evaluationService->evaluation(
        classId,
        currentEvaluation
        );
    return forTermSchedule(
        calendar.schedule(),
        schoolLevel,
        date,
        rows && isPopulated(*rows)
        );
}

} // namespace EvaluationDefaultSelection

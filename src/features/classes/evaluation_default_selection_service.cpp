#include "features/classes/evaluation_default_selection.h"

#include "app/services/feature_services.h"
#include "classmngr/engine/evaluation_default_selection.h"
#include "core/application_services.h"
#include "domain/models/class_info.h"
#include "features/calendar/ui/academic_calendar_provider.h"
#include "features/classes/class_navigation_preferences.h"

namespace
{
using EngineSelection = classmngr::engine::EvaluationDefaultSelection;

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

    SettingsService* settingsService = services->settingsService();
    if (
        ClassNavigationPreferences::evaluationDefaultPolicy(settingsService)
        != ClassNavigationPreferences::EvaluationDefaultPolicy::CurrentOrPreviousTerm
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

    const QString grade = classService->classInfo(classId)
        .value_or(ClassInfo{})
        .classGrade;
    const classmngr::engine::SchoolLevel engineSchoolLevel =
        EngineSelection::schoolLevelForGrade(
            grade.toUtf8().toStdString()
            );
    const SchoolLevel schoolLevel =
        engineSchoolLevel == classmngr::engine::SchoolLevel::Middle
            ? SchoolLevel::Middle
            : SchoolLevel::Elementary;
    AcademicCalendarProvider calendar(settingsService);
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

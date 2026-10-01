#include "features/classes/evaluation_default_selection.h"

#include "core/application_services.h"
#include "features/calendar/ui/academic_calendar_provider.h"
#include "next/application/evaluation_default_selection.h"
#include "next/application/selected_class_grade_read_query.h"
#include "next/application/speaking_evaluation_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_selected_class_grade_read_port.h"
#include "next/platform/application_services_speaking_evaluation_read_port.h"
#include "next/platform/application_services_evaluation_default_policy_port.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"
#include "next/platform/application_services_calendar_first_day_of_week_preferences_port.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace EvaluationDefaultSelection::Private
{
[[nodiscard]] SchoolLevel schoolLevelForClassGrade(const QString& grade);
}

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

    const std::optional<ClassMngr::Next::Domain::ClassId> typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classId)
            );
    if (!typedClassId)
    {
        return {};
    }

    const ClassMngr::Next::Platform::
        ApplicationServicesSelectedClassGradeReadPort classGradePort(*services);
    const ClassMngr::Next::Application::SelectedClassGradeReadQuery classGradeQuery(
        classGradePort
        );
    const auto classGrade = classGradeQuery.execute(*typedClassId);
    if (!classGrade)
    {
        return {};
    }

    const std::string& gradeUtf8 = classGrade.value().classGrade;
    const SchoolLevel schoolLevel = Private::schoolLevelForClassGrade(
        QString::fromUtf8(
            gradeUtf8.data(),
            static_cast<qsizetype>(gradeUtf8.size())
            )
        );
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
    const ClassMngr::Next::Application::SpeakingEvaluationReadQuery
        evaluationQuery{
            *typedClassId,
            currentEvaluation.toStdU16String()
        };
    const ClassMngr::Next::Platform::
        ApplicationServicesSpeakingEvaluationReadPort evaluationPort(*services);
    const auto evaluation =
        ClassMngr::Next::Application::SpeakingEvaluationQuery::execute(
            evaluationQuery,
            evaluationPort
        );
    if (!evaluation)
    {
        return {};
    }

    return forTermSchedule(
        calendar.schedule(),
        schoolLevel,
        date,
        ClassMngr::Next::Application::evaluationRowsHaveContent(
            evaluation.value().rows
            )
        );
}

} // namespace EvaluationDefaultSelection

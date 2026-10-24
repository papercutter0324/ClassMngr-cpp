#pragma once

#include "next/platform/application_services_teacher_import_apply_port.h"

#include <variant>

namespace ClassMngr::Features::TeacherUi
{

[[nodiscard]] inline QString teacherImportApplyErrorDisplayText(
    const ClassMngr::Next::Platform::TeacherImportApplyPortError& error
    )
{
    using namespace ClassMngr::Next;

    if (const auto* preparationFailure = std::get_if<
            Platform::TeacherImportPlanPreparationFailure>(&error))
    {
        return preparationFailure->diagnostic;
    }
    if (std::holds_alternative<
            Platform::TeacherImportWorkspaceUnavailableFailure>(error))
    {
        return QStringLiteral("No Teacher Profile service is available.");
    }
    return TeacherImportRepository::legacyErrorMessage(
        std::get<Application::TeacherImportUseCaseError>(error));
}

} // namespace ClassMngr::Features::TeacherUi

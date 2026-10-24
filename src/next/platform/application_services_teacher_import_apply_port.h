#pragma once

#include "core/application_services.h"
#include "core/result.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_import_legacy_plan_adapter.h"
#include "data/repositories/teacher_import_repository.h"
#include "domain/validation/teacher_import_plan_preparation.h"

namespace ClassMngr::Next::Platform
{

class ApplicationServicesTeacherImportApplyPort final
{
public:
    explicit ApplicationServicesTeacherImportApplyPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Result<Application::TeacherImportUseCaseResult> apply(
        const TeacherImportPlan& plan
        ) const
    {
        const auto normalizedPlan =
            ClassMngr::Domain::prepareTeacherImportPlan(plan);
        if (!normalizedPlan)
        {
            return std::unexpected(normalizedPlan.error());
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return std::unexpected(
                QStringLiteral("No Teacher Profile service is available."));
        }

        TeacherImportRepository* const repository =
            store->teacherImportRepository();
        if (!repository)
        {
            return std::unexpected(
                QStringLiteral("No Teacher Profile service is available."));
        }

        const Application::TeacherImportUseCaseRequest request =
            ClassMngr::Data::Legacy::teacherImportPlanToUseCaseRequest(
                *normalizedPlan);
        const auto imported = repository->applyUseCase(request);
        if (!imported)
        {
            return std::unexpected(
                TeacherImportRepository::legacyErrorMessage(imported.error()));
        }

        return *imported;
    }

private:
    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform

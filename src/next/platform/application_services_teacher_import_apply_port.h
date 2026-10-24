#pragma once

#include "core/application_services.h"
#include "core/result.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_import_legacy_plan_adapter.h"
#include "data/repositories/teacher_import_repository.h"
#include "domain/validation/teacher_import_plan_preparation.h"

#include <QString>

#include <expected>
#include <variant>

namespace ClassMngr::Next::Platform
{

struct TeacherImportPlanPreparationFailure final
{
    QString diagnostic;

    friend bool operator==(
        const TeacherImportPlanPreparationFailure&,
        const TeacherImportPlanPreparationFailure&
        ) = default;
};

struct TeacherImportWorkspaceUnavailableFailure final
{
    friend bool operator==(
        const TeacherImportWorkspaceUnavailableFailure&,
        const TeacherImportWorkspaceUnavailableFailure&
        ) = default;
};

using TeacherImportApplyPortError = std::variant<
    TeacherImportPlanPreparationFailure,
    TeacherImportWorkspaceUnavailableFailure,
    Application::TeacherImportUseCaseError
    >;

using TeacherImportApplyPortResult = std::expected<
    Application::TeacherImportUseCaseResult,
    TeacherImportApplyPortError
    >;

class ApplicationServicesTeacherImportApplyPort final
{
public:
    explicit ApplicationServicesTeacherImportApplyPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] TeacherImportApplyPortResult apply(
        const TeacherImportPlan& plan
        ) const
    {
        const auto normalizedPlan =
            ClassMngr::Domain::prepareTeacherImportPlan(plan);
        if (!normalizedPlan)
        {
            return std::unexpected(TeacherImportPlanPreparationFailure{
                .diagnostic = normalizedPlan.error()
            });
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return std::unexpected(
                TeacherImportWorkspaceUnavailableFailure{});
        }

        TeacherImportRepository* const repository =
            store->teacherImportRepository();
        if (!repository)
        {
            return std::unexpected(
                TeacherImportWorkspaceUnavailableFailure{});
        }

        const Application::TeacherImportUseCaseRequest request =
            ClassMngr::Data::Legacy::teacherImportPlanToUseCaseRequest(
                *normalizedPlan);
        const auto imported = repository->applyUseCase(request);
        if (!imported)
        {
            return std::unexpected(imported.error());
        }

        return *imported;
    }

private:
    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform

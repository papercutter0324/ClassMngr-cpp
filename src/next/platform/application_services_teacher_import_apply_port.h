#pragma once

#include "core/application_services.h"
#include "core/result.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_import_repository.h"
#include "domain/validation/teacher_import_plan_preparation.h"
#include "next/application/teacher_import_plan_mapper.h"
#include "data/repositories/teacher_import_sql_persistence_adapter.h"

#include <QSqlDatabase>

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

    [[nodiscard]] Result<TeacherImportSummary> apply(
        const TeacherImportPlan& plan
        ) const
    {
        const auto normalizedPlan =
            ClassMngr::Domain::prepareTeacherImportPlan(plan);
        if (!normalizedPlan)
        {
            return std::unexpected(normalizedPlan.error());
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return std::unexpected(
                QStringLiteral("No Teacher Profile service is available."));
        }

        QSqlDatabase database = session->database();
        const auto persistence = makeTeacherImportSqlPersistenceAdapter(database);
        const Application::TeacherImportUseCaseRequest request =
            Application::teacherImportPlanToUseCaseRequest(*normalizedPlan);
        const auto imported =
            Application::TeacherImportUseCase::execute(request, *persistence);
        if (!imported)
        {
            TeacherImportRepository::logRollbackFailure(imported.error());
            return std::unexpected(
                TeacherImportRepository::legacyErrorMessage(imported.error()));
        }

        return TeacherImportRepository::legacySummary(*imported);
    }

private:
    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform

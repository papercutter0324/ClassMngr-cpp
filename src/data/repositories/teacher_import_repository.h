#pragma once

#include "data/repositories/repository_error.h"
#include "domain/models/teacher_import.h"
#include "next/application/teacher_import_use_case.h"

#include <QSqlDatabase>
#include <QString>

#include <expected>

class TeacherImportRepository
{
public:
    static constexpr auto LatestSourceDateSetting =
        "teacher_import/latest_source_date";

    explicit TeacherImportRepository(QSqlDatabase& database);

    [[nodiscard]] RepositoryResult<TeacherImportSummary> importTeachers(
        const TeacherImportPlan& plan
        );

    // Transitional application orchestration bridge, separate from the CRUD
    // RepositoryResult contract above. Remove it when
    // ApplicationServicesTeacherImportApplyPort composes TeacherImportUseCase
    // over the persistence adapter/port directly.
    [[nodiscard]] std::expected<
        ClassMngr::Next::Application::TeacherImportUseCaseResult,
        ClassMngr::Next::Application::TeacherImportUseCaseError
        > applyUseCase(
            const ClassMngr::Next::Application::TeacherImportUseCaseRequest& request
            );

    [[nodiscard]] static QString legacyErrorMessage(
        const ClassMngr::Next::Application::TeacherImportUseCaseError& error
        );
    [[nodiscard]] static TeacherImportSummary legacySummary(
        const ClassMngr::Next::Application::TeacherImportUseCaseResult& result
        );
    static void logRollbackFailure(
        const ClassMngr::Next::Application::TeacherImportUseCaseError& error
        );

private:
    QSqlDatabase& m_database;
};

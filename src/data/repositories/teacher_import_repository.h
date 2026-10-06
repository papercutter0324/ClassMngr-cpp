#pragma once

#include "core/result.h"
#include "domain/models/teacher_import.h"
#include "next/application/teacher_import_use_case.h"

#include <QSqlDatabase>
#include <QString>

class TeacherImportRepository
{
public:
    static constexpr auto LatestSourceDateSetting =
        "teacher_import/latest_source_date";

    explicit TeacherImportRepository(QSqlDatabase& database);

    [[nodiscard]] Result<TeacherImportSummary> importTeachers(
        const TeacherImportPlan& plan
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

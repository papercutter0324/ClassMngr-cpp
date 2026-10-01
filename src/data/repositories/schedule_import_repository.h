#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_apply_use_case.h"

#include <QSqlDatabase>

class ScheduleImportRepository
{
public:
    explicit ScheduleImportRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] Result<ScheduleImportPreview> preview(
        const ScheduleImportUserBlock& user,
        ScheduleImportKind kind
        );

    [[nodiscard]] Result<ScheduleImportSummary> apply(
        const ScheduleImportPlan& plan
        );

    // Interim typed adapter for the v2 ApplyUseCase boundary. Mapping stays
    // here so persistence validation and transactional writes remain shared
    // with the legacy plan-backed apply core below.
    [[nodiscard]] Result<ScheduleImportSummary> applyTyped(
        const ClassMngr::Next::Application::ScheduleImportApplyRequest& request
        );

private:
    QSqlDatabase& m_database;
};

#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_apply_request.h"

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

    [[nodiscard]] Result<ScheduleImportSummary> applyTyped(
        const ClassMngr::Next::Application::ScheduleImportApplyRequest& request
        );

private:
    [[nodiscard]] Result<ScheduleImportSummary> applyCore(
        const ClassMngr::Next::Application::ScheduleImportApplyRequest& request
        );

    QSqlDatabase& m_database;
};

#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"

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

private:
    QSqlDatabase& m_database;
};

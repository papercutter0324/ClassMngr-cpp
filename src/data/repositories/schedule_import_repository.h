#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_state_validation.h"

#include <QSqlDatabase>
#include <QString>

#include <expected>
#include <optional>
#include <utility>

using ScheduleImportTypedApplyResult = std::expected<
    ScheduleImportSummary,
    ClassMngr::Next::Application::ScheduleImportApplyFailure
    >;

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

    [[nodiscard]] ScheduleImportTypedApplyResult applyTyped(
        const ClassMngr::Next::Application::ScheduleImportApplyRequest& request
        );

private:
    struct ApplyCoreFailure final
    {
        QString message;
        std::optional<
            ClassMngr::Next::Application::ScheduleImportStateValidationError
            > stateValidationError;

        ApplyCoreFailure(QString message)
            : message(std::move(message))
        {
        }

        ApplyCoreFailure(
            QString message,
            ClassMngr::Next::Application::ScheduleImportStateValidationError
                stateValidationError
            )
            : message(std::move(message)),
              stateValidationError(std::move(stateValidationError))
        {
        }
    };

    using ApplyCoreResult = std::expected<
        ScheduleImportSummary,
        ApplyCoreFailure
        >;

    [[nodiscard]] ApplyCoreResult applyCore(
        const ClassMngr::Next::Application::ScheduleImportApplyRequest& request
        );

    QSqlDatabase& m_database;
};

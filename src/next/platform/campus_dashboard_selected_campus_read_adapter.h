#pragma once

#include "features/campus/data/campus_json_repository.h"
#include "next/application/campus_dashboard_selected_campus_read_port.h"
#include "next/platform/campus_dashboard_campus_repository_adapter.h"

#include <QByteArray>
#include <QString>

#include <optional>
#include <utility>

namespace ClassMngr::Next::Platform
{

class CampusDashboardSelectedCampusReadAdapter final
    : public Application::CampusDashboardSelectedCampusReadPort
{
public:
    explicit CampusDashboardSelectedCampusReadAdapter(
        QString campusDirectory
        )
        : m_repository(std::move(campusDirectory))
    {
    }

    CampusDashboardSelectedCampusReadAdapter(
        const CampusDashboardSelectedCampusReadAdapter&
        ) = delete;
    CampusDashboardSelectedCampusReadAdapter& operator=(
        const CampusDashboardSelectedCampusReadAdapter&
        ) = delete;
    CampusDashboardSelectedCampusReadAdapter(
        CampusDashboardSelectedCampusReadAdapter&&
        ) = delete;
    CampusDashboardSelectedCampusReadAdapter& operator=(
        CampusDashboardSelectedCampusReadAdapter&&
        ) = delete;

    [[nodiscard]] Application::CampusDashboardSelectedCampusReadResult
    loadCampus(const Domain::CampusId& campusId) const override
    {
        const RepositoryResult<std::optional<CampusInfo>> loaded =
            m_repository.loadCampus(
            QString::fromUtf8(campusId.value())
            );
        if (!loaded)
        {
            const RepositoryError& error = loaded.error();
            Domain::ErrorCode code = Domain::ErrorCode::Technical;
            switch (error.code)
            {
            case RepositoryErrorCode::InvalidInput:
                code = Domain::ErrorCode::InvalidInput;
                break;
            case RepositoryErrorCode::NotFound:
                code = Domain::ErrorCode::NotFound;
                break;
            case RepositoryErrorCode::Conflict:
                code = Domain::ErrorCode::Conflict;
                break;
            case RepositoryErrorCode::Validation:
                code = Domain::ErrorCode::Validation;
                break;
            case RepositoryErrorCode::Technical:
                code = Domain::ErrorCode::Technical;
                break;
            }

            const QByteArray diagnostic = error.diagnostic.toUtf8();
            return Application::CampusDashboardSelectedCampusReadResult::failure(
                Domain::OperationError{
                    .code = code,
                    .message = diagnostic.toStdString(),
                    .recoverable = error.recoverableOverride.value_or(
                        code == Domain::ErrorCode::NotFound
                        )
                }
                );
        }

        if (!loaded->has_value())
        {
            return Application::CampusDashboardSelectedCampusReadResult::success(
                std::nullopt
                );
        }

        const std::optional<Application::CampusDashboardCampusSnapshot>
            snapshot = CampusDashboardCampusRepositoryAdapter::
                snapshotFromCampusInfo(loaded->value());
        if (!snapshot.has_value())
        {
            return Application::CampusDashboardSelectedCampusReadResult::failure(
                Domain::OperationError{
                    .code = Domain::ErrorCode::Technical,
                    .message = "Campus record has no usable identifier.",
                    .recoverable = false
                }
                );
        }

        return Application::CampusDashboardSelectedCampusReadResult::success(
            snapshot.value()
            );
    }

private:
    CampusJsonRepository m_repository;
};

} // namespace ClassMngr::Next::Platform

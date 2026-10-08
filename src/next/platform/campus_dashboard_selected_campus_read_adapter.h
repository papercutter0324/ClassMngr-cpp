#pragma once

#include "features/campus/data/campus_json_repository.h"
#include "next/application/campus_dashboard_selected_campus_read_port.h"
#include "next/platform/campus_dashboard_campus_repository_adapter.h"

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
        const std::optional<CampusInfo> loaded = m_repository.loadCampus(
            QString::fromUtf8(campusId.value())
            );
        if (!loaded.has_value())
        {
            return Application::CampusDashboardSelectedCampusReadResult::success(
                std::nullopt
                );
        }

        const std::optional<Application::CampusDashboardCampusSnapshot>
            snapshot = CampusDashboardCampusRepositoryAdapter::
                snapshotFromCampusInfo(loaded.value());
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

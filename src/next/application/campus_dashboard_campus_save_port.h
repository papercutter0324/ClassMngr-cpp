#pragma once

#include "next/application/campus_dashboard_campus_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

class CampusDashboardCampusSavePort
{
public:
    virtual ~CampusDashboardCampusSavePort() = default;

    [[nodiscard]] virtual Domain::Result<void> saveCampus(
        const CampusDashboardCampusSnapshot& campus
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

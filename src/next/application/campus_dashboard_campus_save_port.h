#pragma once

#include "next/application/campus_dashboard_campus_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

enum class CampusDashboardCampusSaveIntent
{
    Create,
    Update
};

class CampusDashboardCampusSavePort
{
public:
    virtual ~CampusDashboardCampusSavePort() = default;

    [[nodiscard]] virtual Domain::Result<void> saveCampus(
        const CampusDashboardCampusSnapshot& campus,
        CampusDashboardCampusSaveIntent intent
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

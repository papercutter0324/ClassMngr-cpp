#pragma once

#include "next/application/campus_dashboard_selected_campus_snapshot.h"

namespace ClassMngr::Next::Application
{

class CampusDashboardSelectedCampusReadPort
{
public:
    virtual ~CampusDashboardSelectedCampusReadPort() = default;

    [[nodiscard]] virtual CampusDashboardSelectedCampusReadResult
    loadCampus(const Domain::CampusId& campusId) const = 0;
};

} // namespace ClassMngr::Next::Application

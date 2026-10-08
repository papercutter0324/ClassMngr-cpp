#pragma once

#include "next/application/campus_dashboard_campus_snapshot.h"
#include "next/domain/operation_result.h"

#include <optional>

namespace ClassMngr::Next::Application
{

using CampusDashboardSelectedCampusSnapshot =
    CampusDashboardCampusSnapshot;

using CampusDashboardSelectedCampusReadResult =
    Domain::Result<std::optional<CampusDashboardCampusSnapshot>>;

} // namespace ClassMngr::Next::Application

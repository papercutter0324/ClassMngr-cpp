#pragma once

#include "next/application/class_analytics_dashboard_read_query.h"

#include <optional>
#include <string>

namespace ClassMngr::Next::Application
{

struct ClassAnalyticsPageReadRequest final
{
    Domain::ClassId classId;
    // On first entry, the outer adapter resolves the shared saved default.
    // Later requests carry the explicit selector choice; no value means All.
    bool useDefaultSelection = true;
    std::optional<std::u16string> selectedEvaluationName;
};

struct ClassAnalyticsPageReadSnapshot final
{
    Domain::ClassId classId;
    ClassAnalyticsDashboard dashboard;
    std::optional<std::u16string> selectedEvaluationName;
};

using ClassAnalyticsPageReadResult = Domain::Result<ClassAnalyticsPageReadSnapshot>;

} // namespace ClassMngr::Next::Application

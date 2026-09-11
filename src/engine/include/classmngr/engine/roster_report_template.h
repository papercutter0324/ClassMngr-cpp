#pragma once

#include "classmngr/engine/roster_report.h"

#include <vector>

namespace classmngr::engine
{

enum class RosterReportTemplate : int
{
    ByDay = 0,
    Daily = 1,
    PerClassWithExtraInfo = 2
};

enum class RosterReportScope : int
{
    AllClasses = 0,
    CurrentClass = 1,
    SelectedClasses = 2
};

class RosterReportTemplateService final
{
public:
    [[nodiscard]] static std::vector<RosterReportTemplate>
    availableTemplates();

    [[nodiscard]] static RosterReportOrientation orientation(
        RosterReportTemplate reportTemplate
        );

    [[nodiscard]] static std::vector<int> resolveClassIds(
        RosterReportScope scope,
        int currentClassId,
        const std::vector<int>& selectedClassIds,
        const std::vector<int>& availableClassIds
        );
};

} // namespace classmngr::engine

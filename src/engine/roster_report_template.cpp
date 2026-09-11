#include "classmngr/engine/roster_report_template.h"

#include <algorithm>

namespace classmngr::engine
{
namespace
{
std::vector<int> positiveUniqueIds(const std::vector<int>& ids)
{
    std::vector<int> result;
    result.reserve(ids.size());
    for (const int id : ids)
    {
        if (id > 0
            && std::find(result.begin(), result.end(), id) == result.end())
        {
            result.push_back(id);
        }
    }
    return result;
}
} // namespace

std::vector<RosterReportTemplate>
RosterReportTemplateService::availableTemplates()
{
    return {
        RosterReportTemplate::ByDay,
        RosterReportTemplate::Daily,
        RosterReportTemplate::PerClassWithExtraInfo
    };
}

RosterReportOrientation RosterReportTemplateService::orientation(
    RosterReportTemplate reportTemplate
    )
{
    switch (reportTemplate)
    {
    case RosterReportTemplate::Daily:
    case RosterReportTemplate::PerClassWithExtraInfo:
        return RosterReportOrientation::Portrait;

    case RosterReportTemplate::ByDay:
    default:
        return RosterReportOrientation::Landscape;
    }
}

std::vector<int> RosterReportTemplateService::resolveClassIds(
    RosterReportScope scope,
    int currentClassId,
    const std::vector<int>& selectedClassIds,
    const std::vector<int>& availableClassIds
    )
{
    switch (scope)
    {
    case RosterReportScope::CurrentClass:
        if (currentClassId > 0)
        {
            return {currentClassId};
        }
        return {};

    case RosterReportScope::SelectedClasses:
        return positiveUniqueIds(selectedClassIds);

    case RosterReportScope::AllClasses:
    default:
        return positiveUniqueIds(availableClassIds);
    }
}

} // namespace classmngr::engine

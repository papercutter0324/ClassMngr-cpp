#include "classmngr/engine/roster_report_template.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using namespace classmngr::engine;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineRosterReportTemplateTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        RosterReportTemplateService::availableTemplates()
            == std::vector<RosterReportTemplate>{
                RosterReportTemplate::ByDay,
                RosterReportTemplate::Daily,
                RosterReportTemplate::PerClassWithExtraInfo
            },
        "available template order changed"
        );

    passed &= expect(
        RosterReportTemplateService::orientation(
            RosterReportTemplate::ByDay
            ) == RosterReportOrientation::Landscape
            && RosterReportTemplateService::orientation(
                RosterReportTemplate::Daily
                ) == RosterReportOrientation::Portrait
            && RosterReportTemplateService::orientation(
                RosterReportTemplate::PerClassWithExtraInfo
                ) == RosterReportOrientation::Portrait,
        "template orientation mapping changed"
        );

    passed &= expect(
        RosterReportTemplateService::orientation(
            static_cast<RosterReportTemplate>(-1)
            ) == RosterReportOrientation::Landscape,
        "unknown template orientation fallback changed"
        );

    const std::vector<int> availableClassIds = {
        10, -1, 20, 10, 0, 30, 20
    };
    passed &= expect(
        RosterReportTemplateService::resolveClassIds(
            RosterReportScope::AllClasses,
            20,
            {},
            availableClassIds
            ) == std::vector<int>{10, 20, 30},
        "all-class scope did not filter and deduplicate in input order"
        );

    passed &= expect(
        RosterReportTemplateService::resolveClassIds(
            RosterReportScope::CurrentClass,
            20,
            {30, 10},
            availableClassIds
            ) == std::vector<int>{20}
            && RosterReportTemplateService::resolveClassIds(
                RosterReportScope::CurrentClass,
                0,
                {},
                availableClassIds
                ).empty(),
        "current-class scope did not keep only a positive current id"
        );

    passed &= expect(
        RosterReportTemplateService::resolveClassIds(
            RosterReportScope::SelectedClasses,
            20,
            {-1, 30, 30, 0, 999, 10, 999},
            availableClassIds
            ) == std::vector<int>{30, 999, 10},
        "selected-class scope did not preserve positive unique input ids"
        );

    passed &= expect(
        RosterReportTemplateService::resolveClassIds(
            static_cast<RosterReportScope>(-1),
            20,
            {},
            availableClassIds
            ) == std::vector<int>{10, 20, 30},
        "unknown scope fallback changed"
        );

    return passed ? 0 : 1;
}

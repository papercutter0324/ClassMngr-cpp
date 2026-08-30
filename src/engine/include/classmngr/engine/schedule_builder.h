#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/schedule_report.h"

#include <string>
#include <vector>

namespace classmngr::engine
{

// Builds the renderer-neutral schedule source model from class information.
// Database access and presentation-type conversion remain outside this
// service so the same time/range rules can be used by both desktop stacks.
class ScheduleBuilderService final
{
public:
    [[nodiscard]] static ScheduleReportBuildResult build(
        const std::vector<ClassInfo>& classInfos,
        bool useIntensive,
        const std::vector<std::string>& visibleDays
        );
};

} // namespace classmngr::engine

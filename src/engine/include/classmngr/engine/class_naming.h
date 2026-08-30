#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/teacher.h"

#include <string>

namespace classmngr::engine
{

// Stable class and teacher labels shared by report/package planning and UI
// adapters.  The service returns UTF-8; presentation layers may localize or
// decorate the returned text at their boundary.
class ClassNamingService final
{
public:
    [[nodiscard]] static std::string classDisplayName(
        const ClassInfo& info,
        const Teacher& teacher
        );

    [[nodiscard]] static std::string teacherDisplayName(
        const Teacher& teacher
        );

    [[nodiscard]] static bool teacherDisplayLessThan(
        const Teacher& left,
        const Teacher& right
        );
};

} // namespace classmngr::engine

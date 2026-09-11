#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/validation_result.h"

#include <string_view>

namespace classmngr::engine::ClassInfoValidator
{

[[nodiscard]] ClassInfo normalized(const ClassInfo& info);
[[nodiscard]] ValidationResult validate(const ClassInfo& info);
[[nodiscard]] ValidationResult validateNotes(
    int classId,
    std::string_view notes,
    std::string_view timeFillerActivities
    );

} // namespace classmngr::engine::ClassInfoValidator

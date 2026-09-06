#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/validation_result.h"

#include <string_view>
#include <vector>

namespace classmngr::engine::ClassTimeValidator
{

[[nodiscard]] ClassTime normalized(const ClassTime& time);
[[nodiscard]] ValidationResult validate(
    const std::vector<ClassTime>& times,
    std::string_view fieldPrefix
    );

} // namespace classmngr::engine::ClassTimeValidator

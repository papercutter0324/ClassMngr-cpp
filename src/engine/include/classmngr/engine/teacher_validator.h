#pragma once

#include "classmngr/engine/teacher.h"
#include "classmngr/engine/validation_result.h"

#include <string>
#include <string_view>

namespace classmngr::engine
{

class TeacherValidator final
{
public:
    [[nodiscard]] static Teacher normalized(const Teacher& teacher);
    [[nodiscard]] static std::string normalizedPhoneNumber(
        std::string_view value
        );
    [[nodiscard]] static ValidationResult validate(const Teacher& teacher);
};

} // namespace classmngr::engine

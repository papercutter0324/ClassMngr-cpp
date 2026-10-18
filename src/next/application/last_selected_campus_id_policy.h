#pragma once

#include "next/domain/domain_types.h"
#include "next/application/qt_compatible_text.h"

#include <cstddef>
#include <string_view>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kMaximumLastSelectedCampusIdLength = 256;

[[nodiscard]] inline bool isValidLastSelectedCampusId(
    const Domain::CampusId& campusId
    ) noexcept
{
    const std::string_view value(campusId.value());
    return value.size() <= kMaximumLastSelectedCampusIdLength
        && !isBlankUtf8Text(value);
}

} // namespace ClassMngr::Next::Application

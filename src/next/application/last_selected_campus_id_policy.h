#pragma once

#include "next/domain/domain_types.h"

#include <algorithm>
#include <cctype>
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
        && !value.empty()
        && std::any_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) == 0;
            }
            );
}

} // namespace ClassMngr::Next::Application

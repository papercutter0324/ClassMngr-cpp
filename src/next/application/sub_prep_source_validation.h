#pragma once

#include "next/application/class_summary_projection.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string_view>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kSubPrepSourceMaxIdentifierLength =
    kSummaryMaxIdentifierLength;

namespace SubPrepSourceValidationDetail
{

[[nodiscard]] inline bool isBlank(const std::string_view value) noexcept
{
    return value.empty()
        || std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
}

template <typename TypedId>
[[nodiscard]] inline bool isValidId(const TypedId& id) noexcept
{
    const auto& value = id.value();
    if (value.empty() || value.size() > kSubPrepSourceMaxIdentifierLength)
    {
        return false;
    }

    return !isBlank(value);
}

} // namespace SubPrepSourceValidationDetail

} // namespace ClassMngr::Next::Application

#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline bool isCanonicalPositiveDecimalId(
    const std::string_view value
    )
{
    if (value.empty() || value.front() < '1' || value.front() > '9')
    {
        return false;
    }

    int parsed = 0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
        );
    return error == std::errc{}
        && end == value.data() + value.size()
        && parsed > 0
        && std::to_string(parsed) == value;
}

} // namespace ClassMngr::Next::Application

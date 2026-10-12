#pragma once

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline std::optional<int> parseCanonicalPositiveDecimalId(
    const std::string_view value
    )
{
    if (value.empty() || value.front() < '1' || value.front() > '9')
    {
        return std::nullopt;
    }

    int parsed = 0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
        );
    if (error != std::errc{}
        || end != value.data() + value.size()
        || parsed <= 0
        || std::to_string(parsed) != value)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] inline bool isCanonicalPositiveDecimalId(
    const std::string_view value
    )
{
    return parseCanonicalPositiveDecimalId(value).has_value();
}

} // namespace ClassMngr::Next::Application

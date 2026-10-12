#pragma once

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace ClassMngr::Next::Application
{

[[nodiscard]] inline std::optional<int> parsePositiveDecimalId(
    const std::string_view value
    )
{
    if (value.empty())
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
        || parsed <= 0)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] inline std::optional<int> parseCanonicalPositiveDecimalId(
    const std::string_view value
    )
{
    const std::optional<int> parsed = parsePositiveDecimalId(value);
    if (!parsed || std::to_string(*parsed) != value)
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

#pragma once

#include <charconv>
#include <cstdint>
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

[[nodiscard]] inline std::optional<std::int64_t> parsePositiveDecimalId64(
    const std::string_view value
    )
{
    if (value.empty())
    {
        return std::nullopt;
    }

    std::int64_t parsed = 0;
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

[[nodiscard]] inline std::optional<std::int64_t>
parseCanonicalPositiveDecimalId64(
    const std::string_view value
    )
{
    const std::optional<std::int64_t> parsed = parsePositiveDecimalId64(value);
    if (!parsed || std::to_string(*parsed) != value)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] inline bool isCanonicalPositiveDecimalId64(
    const std::string_view value
    )
{
    return parseCanonicalPositiveDecimalId64(value).has_value();
}

} // namespace ClassMngr::Next::Application

#pragma once

#include <cstdint>

namespace ClassMngr::Next::Application
{

// Application-owned weekday values keep Sub Prep requests independent of Qt
// and the localized day labels used by the current page.
enum class SubPrepWeekday : std::uint8_t
{
    Monday = 1,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

[[nodiscard]] inline constexpr bool isValidSubPrepWeekday(
    const SubPrepWeekday weekday
    ) noexcept
{
    const auto value = static_cast<std::uint8_t>(weekday);
    return value >= static_cast<std::uint8_t>(SubPrepWeekday::Monday)
        && value <= static_cast<std::uint8_t>(SubPrepWeekday::Sunday);
}

} // namespace ClassMngr::Next::Application

#pragma once

#include "next/application/qt_compatible_text.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kCalendarEventTitleMaxUtf16Units = 255;
// A 255-unit title uses at most 765 UTF-8 bytes: three per BMP unit, while
// each supplementary scalar uses four bytes for two UTF-16 units.
inline constexpr std::size_t kCalendarEventTitleMaxUtf8Bytes = 765;

namespace CalendarEventTitleValidation
{

namespace Detail
{

[[nodiscard]] inline bool decodeNextUtf8CodePoint(
    const std::string_view value,
    std::size_t& offset,
    std::uint32_t& codePoint
    ) noexcept
{
    return QtCompatibleTextDetail::decodeNextUtf8CodePoint(
        value,
        offset,
        codePoint
        );
}

} // namespace Detail

// Returns true for empty text or text made only of the Unicode whitespace
// characters trimmed by QString::trimmed(). Malformed UTF-8 returns false;
// the length validator below remains responsible for rejecting it.
[[nodiscard]] inline bool isBlankUtf8Title(
    const std::string_view value
    ) noexcept
{
    return isBlankUtf8Text(value);
}

// Validates UTF-8 while counting Unicode scalar values as UTF-16 code units:
// BMP scalars count as one unit and supplementary scalars count as two.
// Malformed UTF-8 is rejected.
[[nodiscard]] inline bool isValidUtf8WithinUtf16Units(
    const std::string_view value,
    const std::size_t maximumUnits
    ) noexcept
{
    std::size_t offset = 0;
    std::size_t utf16Units = 0;

    while (offset < value.size())
    {
        std::uint32_t codePoint = 0;
        if (!Detail::decodeNextUtf8CodePoint(value, offset, codePoint))
        {
            return false;
        }

        const std::size_t scalarUnits = codePoint > 0xFFFFU ? 2U : 1U;
        if (utf16Units > maximumUnits
            || scalarUnits > maximumUnits - utf16Units)
        {
            return false;
        }

        utf16Units += scalarUnits;
    }

    return true;
}

} // namespace CalendarEventTitleValidation
} // namespace ClassMngr::Next::Application

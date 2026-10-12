#pragma once

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
        const auto lead = static_cast<unsigned char>(value[offset]);
        std::size_t byteCount = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimumCodePoint = 0;

        if (lead <= 0x7FU)
        {
            byteCount = 1;
            codePoint = lead;
        }
        else if (lead >= 0xC2U && lead <= 0xDFU)
        {
            byteCount = 2;
            codePoint = lead & 0x1FU;
            minimumCodePoint = 0x80U;
        }
        else if (lead >= 0xE0U && lead <= 0xEFU)
        {
            byteCount = 3;
            codePoint = lead & 0x0FU;
            minimumCodePoint = 0x800U;
        }
        else if (lead >= 0xF0U && lead <= 0xF4U)
        {
            byteCount = 4;
            codePoint = lead & 0x07U;
            minimumCodePoint = 0x10000U;
        }
        else
        {
            return false;
        }

        if (value.size() - offset < byteCount)
        {
            return false;
        }

        for (std::size_t index = 1; index < byteCount; ++index)
        {
            const auto continuation =
                static_cast<unsigned char>(value[offset + index]);
            if ((continuation & 0xC0U) != 0x80U)
            {
                return false;
            }

            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }

        if (codePoint < minimumCodePoint
            || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
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
        offset += byteCount;
    }

    return true;
}

} // namespace CalendarEventTitleValidation
} // namespace ClassMngr::Next::Application

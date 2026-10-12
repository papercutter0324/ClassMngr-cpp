#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

namespace QtCompatibleTextDetail
{

[[nodiscard]] inline bool decodeNextUtf8CodePoint(
    const std::string_view value,
    std::size_t& offset,
    std::uint32_t& codePoint
    ) noexcept
{
    if (offset >= value.size())
    {
        return false;
    }

    const auto lead = static_cast<unsigned char>(value[offset]);
    std::size_t byteCount = 0;
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

    offset += byteCount;
    return true;
}

} // namespace QtCompatibleTextDetail

// Matches the Unicode whitespace characters trimmed by QString::trimmed().
[[nodiscard]] inline bool isQtWhitespace(char16_t value) noexcept
{
    return (value >= u'\t' && value <= u'\r')
        || value == u' '
        || value == 0x0085
        || value == 0x00a0
        || value == 0x1680
        || (value >= 0x2000 && value <= 0x200a)
        || value == 0x2028
        || value == 0x2029
        || value == 0x202f
        || value == 0x205f
        || value == 0x3000;
}

[[nodiscard]] inline std::u16string trimQtWhitespace(
    std::u16string_view value
    )
{
    std::size_t first = 0;
    while (first < value.size() && isQtWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isQtWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::u16string(value.substr(first, last - first));
}

// Returns true for empty text or valid UTF-8 made only of the Unicode
// whitespace characters trimmed by QString::trimmed(). Malformed UTF-8 is
// nonblank so callers that validate UTF-8 can retain responsibility for it.
[[nodiscard]] inline bool isBlankUtf8Text(
    const std::string_view value
    ) noexcept
{
    std::size_t offset = 0;
    while (offset < value.size())
    {
        std::uint32_t codePoint = 0;
        if (!QtCompatibleTextDetail::decodeNextUtf8CodePoint(
                value,
                offset,
                codePoint
                )
            || codePoint > 0xFFFFU
            || !isQtWhitespace(static_cast<char16_t>(codePoint)))
        {
            return false;
        }
    }

    return true;
}

} // namespace ClassMngr::Next::Application

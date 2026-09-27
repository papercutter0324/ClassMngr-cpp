#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ClassMngr::Next::Application
{

// Calendar display adapters provide titles and event types as UTF-8. The
// aliases contain only ASCII letters, and Qt's lowercase mappings do not map
// non-ASCII letters to any of those alias letters. ASCII lowercase conversion
// is therefore sufficient for these comparisons without bringing Qt into the
// Application layer.
struct CalendarEventStartOfTermPolicy final
{
    [[nodiscard]] static bool isStartOfTermEvent(
        const std::string_view title,
        const std::string_view eventType
        ) noexcept
    {
        return normalizedEventType(eventType) == "Other"
            && hasStartOfTermTitle(title);
    }

    [[nodiscard]] static bool shouldHideEvent(
        const std::string_view title,
        const std::string_view eventType,
        const bool hideStartOfTermEvents
        ) noexcept
    {
        return hideStartOfTermEvents
            && isStartOfTermEvent(title, eventType);
    }

private:
    struct DecodedCodePoint final
    {
        std::uint32_t value = 0;
        std::size_t byteCount = 1;
        bool valid = false;
    };

    [[nodiscard]] static constexpr bool isContinuationByte(
        const unsigned char value
        ) noexcept
    {
        return (value & 0xC0U) == 0x80U;
    }

    [[nodiscard]] static DecodedCodePoint decodeCodePoint(
        const std::string_view value,
        const std::size_t offset
        ) noexcept
    {
        const auto first = static_cast<unsigned char>(value[offset]);
        if (first <= 0x7FU)
        {
            return {first, 1, true};
        }

        std::size_t byteCount = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            byteCount = 2;
            codePoint = first & 0x1FU;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            byteCount = 3;
            codePoint = first & 0x0FU;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            byteCount = 4;
            codePoint = first & 0x07U;
            minimum = 0x10000U;
        }
        else
        {
            return {0xFFFDU, 1, false};
        }

        if (offset + byteCount > value.size())
        {
            return {0xFFFDU, 1, false};
        }

        for (std::size_t index = 1; index < byteCount; ++index)
        {
            const auto next = static_cast<unsigned char>(
                value[offset + index]
                );
            if (!isContinuationByte(next))
            {
                return {0xFFFDU, 1, false};
            }
            codePoint = (codePoint << 6U) | (next & 0x3FU);
        }

        if (codePoint < minimum
            || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
        {
            return {0xFFFDU, 1, false};
        }

        return {codePoint, byteCount, true};
    }

    // These are QChar::isSpace() whitespace values used by QString's
    // trimmed()/simplified() behavior: ASCII tab through CR, NEL, and Unicode
    // separator characters.
    [[nodiscard]] static constexpr bool isQtSpace(
        const std::uint32_t codePoint
        ) noexcept
    {
        return (codePoint >= 0x0009U && codePoint <= 0x000DU)
            || codePoint == 0x0085U
            || codePoint == 0x0020U
            || codePoint == 0x00A0U
            || codePoint == 0x1680U
            || (codePoint >= 0x2000U && codePoint <= 0x200AU)
            || codePoint == 0x2028U
            || codePoint == 0x2029U
            || codePoint == 0x202FU
            || codePoint == 0x205FU
            || codePoint == 0x3000U;
    }

    [[nodiscard]] static std::string_view trimQtSpace(
        const std::string_view value
        ) noexcept
    {
        std::size_t first = 0;
        while (first < value.size())
        {
            const DecodedCodePoint codePoint =
                decodeCodePoint(value, first);
            if (!codePoint.valid || !isQtSpace(codePoint.value))
            {
                break;
            }
            first += codePoint.byteCount;
        }

        std::size_t last = value.size();
        while (last > first)
        {
            std::size_t codePointStart = last - 1;
            while (codePointStart > first
                && isContinuationByte(static_cast<unsigned char>(
                    value[codePointStart]
                    )))
            {
                --codePointStart;
            }

            const DecodedCodePoint codePoint =
                decodeCodePoint(value, codePointStart);
            if (!codePoint.valid
                || codePointStart + codePoint.byteCount != last
                || !isQtSpace(codePoint.value))
            {
                break;
            }
            last = codePointStart;
        }

        return value.substr(first, last - first);
    }

    [[nodiscard]] static std::string_view normalizedEventType(
        const std::string_view eventType
        ) noexcept
    {
        static constexpr std::array<std::string_view, 6> knownTypes{
            "Vacation",
            "Holiday",
            "Workshop",
            "CM",
            "Meeting",
            "Other"
        };

        const std::string_view trimmed = trimQtSpace(eventType);
        for (const std::string_view knownType : knownTypes)
        {
            if (trimmed == knownType)
            {
                return knownType;
            }
        }
        return "Other";
    }

    [[nodiscard]] static bool hasStartOfTermTitle(
        const std::string_view title
        ) noexcept
    {
        static constexpr std::array<std::string_view, 4> aliases{
            "new semester",
            "start of term",
            "term start",
            "term starts"
        };

        for (const std::string_view alias : aliases)
        {
            if (normalizedTitleEquals(title, alias))
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] static bool normalizedTitleEquals(
        const std::string_view title,
        const std::string_view alias
        ) noexcept
    {
        std::size_t aliasIndex = 0;
        bool hasCharacter = false;
        bool pendingSpace = false;
        for (std::size_t offset = 0; offset < title.size();)
        {
            const DecodedCodePoint codePoint =
                decodeCodePoint(title, offset);
            offset += codePoint.byteCount;

            if (codePoint.valid && isQtSpace(codePoint.value))
            {
                if (hasCharacter)
                {
                    pendingSpace = true;
                }
                continue;
            }

            if (pendingSpace)
            {
                if (aliasIndex >= alias.size() || alias[aliasIndex] != ' ')
                {
                    return false;
                }
                ++aliasIndex;
                pendingSpace = false;
            }

            std::uint32_t normalized = codePoint.value;
            if (normalized >= 'A' && normalized <= 'Z')
            {
                normalized += 'a' - 'A';
            }
            if (normalized > 0x7FU
                || aliasIndex >= alias.size()
                || alias[aliasIndex]
                    != static_cast<char>(normalized))
            {
                return false;
            }
            ++aliasIndex;
            hasCharacter = true;
        }

        return hasCharacter
            && aliasIndex == alias.size();
    }
};

}

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

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

} // namespace ClassMngr::Next::Application

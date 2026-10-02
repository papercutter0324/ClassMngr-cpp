#pragma once

#include <QString>

#include <string>
#include <string_view>

namespace RosterUi::QtTextAdapter
{

// Copy QString's UTF-16 code units directly. QString's standard-string
// conversion can treat an initial U+FEFF as an encoding marker.
[[nodiscard]] inline std::u16string toUtf16String(
    const QString& value
    )
{
    std::u16string result;
    result.reserve(static_cast<std::size_t>(value.size()));
    for (const QChar character : value)
    {
        result.push_back(static_cast<char16_t>(character.unicode()));
    }
    return result;
}

// Construct the QString from code units so a leading U+FEFF stays content.
[[nodiscard]] inline QString fromUtf16String(
    const std::u16string_view value
    )
{
    QString result;
    result.reserve(static_cast<qsizetype>(value.size()));
    for (const char16_t character : value)
    {
        result.append(QChar(static_cast<ushort>(character)));
    }
    return result;
}

} // namespace RosterUi::QtTextAdapter

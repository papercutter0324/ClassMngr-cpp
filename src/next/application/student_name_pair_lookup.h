#pragma once

#include "next/application/qt_compatible_text.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct StudentNamePairText final
{
    std::u16string englishName;
    std::u16string koreanName;
};

[[nodiscard]] inline StudentNamePairText trimStudentNamePairText(
    const StudentNamePairText& names
    )
{
    return {
        trimQtWhitespace(names.englishName),
        trimQtWhitespace(names.koreanName)
    };
}

// Retains the legacy U+001F separator and its delimiter-collision behavior.
[[nodiscard]] inline std::u16string studentNamePairLookupKey(
    const StudentNamePairText& names
    )
{
    const StudentNamePairText trimmedNames = trimStudentNamePairText(names);
    if (trimmedNames.englishName.empty() || trimmedNames.koreanName.empty())
    {
        return {};
    }

    std::u16string key = trimmedNames.englishName;
    key.push_back(0x001f);
    key += trimmedNames.koreanName;
    return key;
}

// Rows are positional: the selected row is the index into namePairs, and
// matching peers are returned in their original row order.
[[nodiscard]] inline std::vector<int> lookupStudentNamePairPeers(
    const std::vector<StudentNamePairText>& namePairs,
    const int selectedRow
    )
{
    std::vector<int> peers;
    if (selectedRow < 0
        || static_cast<std::size_t>(selectedRow) >= namePairs.size())
    {
        return peers;
    }

    const std::u16string selectedKey =
        studentNamePairLookupKey(namePairs[static_cast<std::size_t>(selectedRow)]);
    if (selectedKey.empty())
    {
        return peers;
    }

    for (std::size_t row = 0; row < namePairs.size(); ++row)
    {
        if (row == static_cast<std::size_t>(selectedRow))
        {
            continue;
        }

        if (studentNamePairLookupKey(namePairs[row]) == selectedKey)
        {
            peers.push_back(static_cast<int>(row));
        }
    }

    return peers;
}

} // namespace ClassMngr::Next::Application

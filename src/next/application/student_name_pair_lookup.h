#pragma once

#include "next/application/qt_compatible_text.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct StudentNamePairText final
{
    std::u16string englishName;
    std::u16string koreanName;
};

class StudentNamePairSelectedRowIndex final
{
public:
    constexpr explicit StudentNamePairSelectedRowIndex(
        int rawValue = -1
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const StudentNamePairSelectedRowIndex&,
        const StudentNamePairSelectedRowIndex&
        ) noexcept = default;

private:
    int m_rawValue;
};

class StudentNamePairPeerRowIndex final
{
public:
    constexpr explicit StudentNamePairPeerRowIndex(
        int rawValue = -1
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const StudentNamePairPeerRowIndex&,
        const StudentNamePairPeerRowIndex&
        ) noexcept = default;

private:
    int m_rawValue;
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
[[nodiscard]] inline std::vector<StudentNamePairPeerRowIndex>
lookupStudentNamePairPeers(
    const std::vector<StudentNamePairText>& namePairs,
    const StudentNamePairSelectedRowIndex selectedRow
    )
{
    std::vector<StudentNamePairPeerRowIndex> peers;
    const int selectedRowValue = selectedRow.rawValue();
    if (selectedRowValue < 0
        || std::cmp_greater_equal(selectedRowValue, namePairs.size()))
    {
        return peers;
    }

    const std::size_t selectedRowIndex =
        static_cast<std::size_t>(selectedRowValue);

    const std::u16string selectedKey =
        studentNamePairLookupKey(
            namePairs[selectedRowIndex]
            );
    if (selectedKey.empty())
    {
        return peers;
    }

    for (std::size_t row = 0; row < namePairs.size(); ++row)
    {
        if (row == selectedRowIndex)
        {
            continue;
        }

        if (studentNamePairLookupKey(namePairs[row]) == selectedKey)
        {
            peers.emplace_back(static_cast<int>(row));
        }
    }

    return peers;
}

} // namespace ClassMngr::Next::Application

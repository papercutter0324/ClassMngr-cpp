#pragma once

#include <compare>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Domain
{

class StudentNamePair final
{
public:
    [[nodiscard]] static std::optional<StudentNamePair> fromNames(
        const std::u16string_view englishName,
        const std::u16string_view koreanName
        )
    {
        if (englishName.empty() || koreanName.empty())
        {
            return std::nullopt;
        }

        return StudentNamePair(
            std::u16string(englishName),
            std::u16string(koreanName)
            );
    }

    [[nodiscard]] const std::u16string& englishName() const noexcept
    {
        return m_englishName;
    }

    [[nodiscard]] const std::u16string& koreanName() const noexcept
    {
        return m_koreanName;
    }

    friend bool operator==(
        const StudentNamePair&,
        const StudentNamePair&
        ) = default;

    friend auto operator<=>(
        const StudentNamePair&,
        const StudentNamePair&
        ) = default;

private:
    StudentNamePair(
        std::u16string englishName,
        std::u16string koreanName
        )
        : m_englishName(std::move(englishName)),
          m_koreanName(std::move(koreanName))
    {
    }

    std::u16string m_englishName;
    std::u16string m_koreanName;
};

struct DuplicateStudentNamePairGroup final
{
    StudentNamePair namePair;
    std::vector<std::size_t> rowIndexes;
};

// Groups complete row values by exact StudentNamePair equality. Empty optionals
// represent rows that the caller considers incomplete. Groups follow the first
// row where each pair appears, and indexes within each group follow input order.
[[nodiscard]] inline std::vector<DuplicateStudentNamePairGroup>
duplicateStudentNamePairGroups(
    const std::vector<std::optional<StudentNamePair>>& namePairsByRow
    )
{
    std::vector<DuplicateStudentNamePairGroup> groups;
    groups.reserve(namePairsByRow.size());
    std::map<StudentNamePair, std::size_t> groupIndexesByPair;

    for (std::size_t rowIndex = 0;
         rowIndex < namePairsByRow.size();
         ++rowIndex)
    {
        const auto& namePair = namePairsByRow[rowIndex];
        if (!namePair)
        {
            continue;
        }

        const auto [groupIndex, inserted] = groupIndexesByPair.emplace(
            *namePair,
            groups.size()
            );
        if (inserted)
        {
            groups.push_back({*namePair, {}});
        }

        groups[groupIndex->second].rowIndexes.push_back(rowIndex);
    }

    std::vector<DuplicateStudentNamePairGroup> duplicates;
    duplicates.reserve(groups.size());
    for (auto& group : groups)
    {
        if (group.rowIndexes.size() > 1)
        {
            duplicates.push_back(std::move(group));
        }
    }

    return duplicates;
}

} // namespace ClassMngr::Next::Domain

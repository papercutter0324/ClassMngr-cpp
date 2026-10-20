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
    StudentNamePair(const StudentNamePair&) = default;
    StudentNamePair& operator=(const StudentNamePair&) = default;

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

class DuplicateStudentNamePairRowIndex final
{
public:
    constexpr DuplicateStudentNamePairRowIndex() noexcept = default;

    explicit constexpr DuplicateStudentNamePairRowIndex(
        const std::size_t rawValue
        ) noexcept
        : m_rawValue(rawValue)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return m_rawValue;
    }

    friend constexpr bool operator==(
        const DuplicateStudentNamePairRowIndex&,
        const DuplicateStudentNamePairRowIndex&
        ) noexcept = default;

private:
    std::size_t m_rawValue = 0;
};

struct DuplicateStudentNamePairGroup final
{
    StudentNamePair namePair;
    std::vector<DuplicateStudentNamePairRowIndex> rowIndexes;
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

        groups[groupIndex->second].rowIndexes.push_back(
            DuplicateStudentNamePairRowIndex{rowIndex}
            );
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

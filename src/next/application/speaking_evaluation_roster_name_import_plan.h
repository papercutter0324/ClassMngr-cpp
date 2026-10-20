#pragma once

#include "next/application/student_name_pair_lookup.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

using SpeakingEvaluationRosterNamePair = StudentNamePairText;

class SpeakingEvaluationRosterNameImportSourceIndex final
{
public:
    constexpr SpeakingEvaluationRosterNameImportSourceIndex() noexcept = default;

    explicit constexpr SpeakingEvaluationRosterNameImportSourceIndex(
        const int value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const SpeakingEvaluationRosterNameImportSourceIndex&,
        const SpeakingEvaluationRosterNameImportSourceIndex&
        ) noexcept = default;

private:
    int m_value = -1;
};

class SpeakingEvaluationRosterNameImportTargetIndex final
{
public:
    constexpr SpeakingEvaluationRosterNameImportTargetIndex() noexcept = default;

    explicit constexpr SpeakingEvaluationRosterNameImportTargetIndex(
        const int value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const SpeakingEvaluationRosterNameImportTargetIndex&,
        const SpeakingEvaluationRosterNameImportTargetIndex&
        ) noexcept = default;

private:
    int m_value = -1;
};

struct SpeakingEvaluationRosterNameEvaluationRow final
{
    SpeakingEvaluationRosterNameImportTargetIndex row;
    SpeakingEvaluationRosterNamePair names;
};

struct SpeakingEvaluationRosterNameImportAssignment final
{
    SpeakingEvaluationRosterNameImportSourceIndex sourceRow;
    SpeakingEvaluationRosterNameImportTargetIndex targetRow;
    SpeakingEvaluationRosterNamePair names;
};

[[nodiscard]] inline std::vector<SpeakingEvaluationRosterNameImportAssignment>
planSpeakingEvaluationRosterNameImport(
    const std::vector<SpeakingEvaluationRosterNamePair>& rosterNames,
    const std::vector<SpeakingEvaluationRosterNameEvaluationRow>& evaluationRows
    )
{
    std::unordered_set<std::u16string> existingNamePairs;
    std::vector<SpeakingEvaluationRosterNameImportTargetIndex> availableTargetRows;
    existingNamePairs.reserve(evaluationRows.size());
    availableTargetRows.reserve(evaluationRows.size());

    for (const auto& evaluationRow : evaluationRows)
    {
        const SpeakingEvaluationRosterNamePair names =
            trimStudentNamePairText(evaluationRow.names);
        const std::u16string key = studentNamePairLookupKey(names);
        if (!key.empty())
        {
            existingNamePairs.insert(key);
        }

        if (names.englishName.empty() && names.koreanName.empty())
        {
            availableTargetRows.push_back(evaluationRow.row);
        }
    }

    std::unordered_set<std::u16string> importedNamePairs;
    importedNamePairs.reserve(rosterNames.size());
    std::vector<SpeakingEvaluationRosterNameImportAssignment> assignments;
    assignments.reserve(
        std::min(rosterNames.size(), availableTargetRows.size())
        );

    std::size_t nextTargetRow = 0;
    for (std::size_t sourceRow = 0; sourceRow < rosterNames.size(); ++sourceRow)
    {
        SpeakingEvaluationRosterNamePair names =
            trimStudentNamePairText(rosterNames[sourceRow]);
        const std::u16string key = studentNamePairLookupKey(names);
        if (
            key.empty()
            || importedNamePairs.contains(key)
            || existingNamePairs.contains(key)
            )
        {
            continue;
        }

        if (nextTargetRow >= availableTargetRows.size())
        {
            break;
        }

        importedNamePairs.insert(key);
        assignments.push_back(
            {
                SpeakingEvaluationRosterNameImportSourceIndex{
                    static_cast<int>(sourceRow)
                },
                availableTargetRows[nextTargetRow++],
                std::move(names)
            }
            );
    }

    return assignments;
}

} // namespace ClassMngr::Next::Application

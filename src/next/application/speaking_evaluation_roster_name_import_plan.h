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

struct SpeakingEvaluationRosterNameEvaluationRow final
{
    int row = -1;
    SpeakingEvaluationRosterNamePair names;
};

struct SpeakingEvaluationRosterNameImportAssignment final
{
    int sourceRow = -1;
    int targetRow = -1;
    SpeakingEvaluationRosterNamePair names;
};

[[nodiscard]] inline std::vector<SpeakingEvaluationRosterNameImportAssignment>
planSpeakingEvaluationRosterNameImport(
    const std::vector<SpeakingEvaluationRosterNamePair>& rosterNames,
    const std::vector<SpeakingEvaluationRosterNameEvaluationRow>& evaluationRows
    )
{
    std::unordered_set<std::u16string> existingNamePairs;
    std::vector<int> availableTargetRows;
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
                static_cast<int>(sourceRow),
                availableTargetRows[nextTargetRow++],
                std::move(names)
            }
            );
    }

    return assignments;
}

} // namespace ClassMngr::Next::Application

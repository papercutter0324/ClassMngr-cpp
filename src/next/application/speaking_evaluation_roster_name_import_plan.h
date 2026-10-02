#pragma once

#include "next/application/qt_compatible_text.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationRosterNamePair final
{
    std::u16string englishName;
    std::u16string koreanName;
};

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

namespace SpeakingEvaluationRosterNameImportDetail
{

[[nodiscard]] inline SpeakingEvaluationRosterNamePair trimmedNames(
    const SpeakingEvaluationRosterNamePair& names
    )
{
    return {
        trimQtWhitespace(names.englishName),
        trimQtWhitespace(names.koreanName)
    };
}

// Keep the legacy U+001F key format, including its delimiter-collision
// behavior, so existing duplicate filtering remains unchanged.
[[nodiscard]] inline std::u16string namePairKey(
    const SpeakingEvaluationRosterNamePair& names
    )
{
    if (names.englishName.empty() || names.koreanName.empty())
    {
        return {};
    }

    std::u16string key = names.englishName;
    key.push_back(0x001f);
    key += names.koreanName;
    return key;
}

} // namespace SpeakingEvaluationRosterNameImportDetail

[[nodiscard]] inline std::vector<SpeakingEvaluationRosterNameImportAssignment>
planSpeakingEvaluationRosterNameImport(
    const std::vector<SpeakingEvaluationRosterNamePair>& rosterNames,
    const std::vector<SpeakingEvaluationRosterNameEvaluationRow>& evaluationRows
    )
{
    using SpeakingEvaluationRosterNameImportDetail::namePairKey;
    using SpeakingEvaluationRosterNameImportDetail::trimmedNames;

    std::unordered_set<std::u16string> existingNamePairs;
    std::vector<int> availableTargetRows;
    existingNamePairs.reserve(evaluationRows.size());
    availableTargetRows.reserve(evaluationRows.size());

    for (const auto& evaluationRow : evaluationRows)
    {
        const SpeakingEvaluationRosterNamePair names =
            trimmedNames(evaluationRow.names);
        const std::u16string key = namePairKey(names);
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
            trimmedNames(rosterNames[sourceRow]);
        const std::u16string key = namePairKey(names);
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

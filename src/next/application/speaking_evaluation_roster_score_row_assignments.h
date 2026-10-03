#pragma once

#include "next/application/speaking_evaluation_roster_score_import_use_case.h"
#include "next/application/student_name_pair_lookup.h"
#include "next/domain/student_name_pair.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationRosterScoreRow final
{
    StudentNamePairText names;
    std::optional<std::u16string> currentGrade;
};

struct SpeakingEvaluationRosterScoreAssignment final
{
    std::size_t rosterRowIndex;
    std::u16string finalGrade;

    friend bool operator==(
        const SpeakingEvaluationRosterScoreAssignment&,
        const SpeakingEvaluationRosterScoreAssignment&
        ) = default;
};

// Plans the changed score cells in roster-row order. Imported score names are
// trimmed here as a defensive measure; the import use case already applies
// the same QString-compatible whitespace policy.
[[nodiscard]] inline std::vector<SpeakingEvaluationRosterScoreAssignment>
speakingEvaluationRosterScoreRowAssignments(
    const std::vector<SpeakingEvaluationRosterScoreRow>& rosterRows,
    const std::vector<SpeakingEvaluationRosterScore>& importedScores
    )
{
    std::map<Domain::StudentNamePair, std::u16string> importedGradesByNamePair;
    for (const SpeakingEvaluationRosterScore& score : importedScores)
    {
        const StudentNamePairText trimmedNames = trimStudentNamePairText({
            score.englishName,
            score.koreanName
        });
        const auto namePair = Domain::StudentNamePair::fromNames(
            trimmedNames.englishName,
            trimmedNames.koreanName
            );
        if (!namePair)
        {
            continue;
        }

        // Keep QHash::insert's legacy behavior: the last imported pair wins.
        importedGradesByNamePair.insert_or_assign(
            *namePair,
            score.finalGrade
            );
    }

    std::vector<SpeakingEvaluationRosterScoreAssignment> assignments;
    assignments.reserve(rosterRows.size());
    for (std::size_t rowIndex = 0; rowIndex < rosterRows.size(); ++rowIndex)
    {
        const SpeakingEvaluationRosterScoreRow& row = rosterRows[rowIndex];
        if (!row.currentGrade)
        {
            continue;
        }

        const StudentNamePairText trimmedNames =
            trimStudentNamePairText(row.names);
        const auto namePair = Domain::StudentNamePair::fromNames(
            trimmedNames.englishName,
            trimmedNames.koreanName
            );
        if (!namePair)
        {
            continue;
        }

        const auto importedGrade = importedGradesByNamePair.find(*namePair);
        if (importedGrade == importedGradesByNamePair.end()
            || *row.currentGrade == importedGrade->second)
        {
            continue;
        }

        assignments.push_back({rowIndex, importedGrade->second});
    }

    return assignments;
}

} // namespace ClassMngr::Next::Application

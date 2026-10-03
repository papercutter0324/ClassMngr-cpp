#include "next/application/speaking_evaluation_roster_score_row_assignments.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::SpeakingEvaluationRosterScore;
using ClassMngr::Next::Application::SpeakingEvaluationRosterScoreAssignment;
using ClassMngr::Next::Application::SpeakingEvaluationRosterScoreRow;
using ClassMngr::Next::Application::speakingEvaluationRosterScoreRowAssignments;

namespace
{

SpeakingEvaluationRosterScoreRow rosterRow(
    std::u16string englishName,
    std::u16string koreanName,
    std::optional<std::u16string> currentGrade = std::u16string{}
    )
{
    return {
        .names = {std::move(englishName), std::move(koreanName)},
        .currentGrade = std::move(currentGrade)
    };
}

SpeakingEvaluationRosterScore importedScore(
    std::u16string englishName,
    std::u16string koreanName,
    std::u16string finalGrade
    )
{
    return {
        std::move(englishName),
        std::move(koreanName),
        std::move(finalGrade)
    };
}

bool exactTrimmedPairsMatch()
{
    const std::vector<SpeakingEvaluationRosterScoreRow> rosterRows{
        rosterRow(u"\u00a0Alex Kim\u3000", u"\t\uAE40\uBBFC\uC9C0 ", u""),
        rosterRow(u"alex Kim", u"\uAE40\uBBFC\uC9C0", u"")
    };
    const std::vector<SpeakingEvaluationRosterScore> importedScores{
        importedScore(u"Alex Kim", u"\uAE40\uBBFC\uC9C0", u"A")
    };

    return speakingEvaluationRosterScoreRowAssignments(
        rosterRows,
        importedScores
        ) == std::vector<SpeakingEvaluationRosterScoreAssignment>{{0, u"A"}};
}

bool lastImportedDuplicateWins()
{
    const std::vector<SpeakingEvaluationRosterScoreRow> rosterRows{
        rosterRow(u"Alex", u"\uAE40\uBBFC\uC9C0", u"C")
    };
    const std::vector<SpeakingEvaluationRosterScore> importedScores{
        importedScore(u"Alex", u"\uAE40\uBBFC\uC9C0", u"B"),
        importedScore(u"Alex", u"\uAE40\uBBFC\uC9C0", u"A")
    };

    return speakingEvaluationRosterScoreRowAssignments(
        rosterRows,
        importedScores
        ) == std::vector<SpeakingEvaluationRosterScoreAssignment>{{0, u"A"}};
}

bool incompleteAndUnmatchedPairsAreSkipped()
{
    const std::vector<SpeakingEvaluationRosterScoreRow> rosterRows{
        rosterRow(u"Alex", u"", u""),
        rosterRow(u"", u"\uAE40\uBBFC\uC9C0", u""),
        rosterRow(u"Alex", u"\uAE40\uBBFC\uC9C0", u""),
        rosterRow(u"Jamie", u"\uBC15\uC9C0\uC218", u"")
    };
    const std::vector<SpeakingEvaluationRosterScore> importedScores{
        importedScore(u"", u"\uAE40\uBBFC\uC9C0", u"B"),
        importedScore(u"Alex", u"", u"B"),
        importedScore(u"Alex", u"\uAE40\uBBFC\uC9C0", u"A")
    };

    return speakingEvaluationRosterScoreRowAssignments(
        rosterRows,
        importedScores
        ) == std::vector<SpeakingEvaluationRosterScoreAssignment>{{2, u"A"}};
}

bool alreadyEqualGradesProduceNoAssignment()
{
    const std::vector<SpeakingEvaluationRosterScoreRow> rosterRows{
        rosterRow(u"Alex", u"\uAE40\uBBFC\uC9C0", u"A")
    };
    const std::vector<SpeakingEvaluationRosterScore> importedScores{
        importedScore(u"Alex", u"\uAE40\uBBFC\uC9C0", u"A")
    };

    return speakingEvaluationRosterScoreRowAssignments(
        rosterRows,
        importedScores
        ).empty();
}

bool assignmentsFollowRosterRowOrder()
{
    const std::vector<SpeakingEvaluationRosterScoreRow> rosterRows{
        rosterRow(u"Alex", u"\uAE40\uBBFC\uC9C0", u""),
        rosterRow(u"Jamie", u"\uBC15\uC9C0\uC218", u""),
        rosterRow(u"Morgan", u"\uC774\uC9C0\uC740", u"")
    };
    const std::vector<SpeakingEvaluationRosterScore> importedScores{
        importedScore(u"Morgan", u"\uC774\uC9C0\uC740", u"C"),
        importedScore(u"Alex", u"\uAE40\uBBFC\uC9C0", u"A"),
        importedScore(u"Jamie", u"\uBC15\uC9C0\uC218", u"B")
    };

    return speakingEvaluationRosterScoreRowAssignments(
        rosterRows,
        importedScores
        ) == std::vector<SpeakingEvaluationRosterScoreAssignment>{
            {0, u"A"},
            {1, u"B"},
            {2, u"C"}
        };
}

} // namespace

int main()
{
    if (!exactTrimmedPairsMatch())
    {
        std::fprintf(stderr, "Exact trimmed pair matching changed.\n");
        return EXIT_FAILURE;
    }
    if (!lastImportedDuplicateWins())
    {
        std::fprintf(stderr, "The last imported duplicate did not win.\n");
        return EXIT_FAILURE;
    }
    if (!incompleteAndUnmatchedPairsAreSkipped())
    {
        std::fprintf(stderr, "Incomplete or unmatched pairs were assigned.\n");
        return EXIT_FAILURE;
    }
    if (!alreadyEqualGradesProduceNoAssignment())
    {
        std::fprintf(stderr, "An already-equal grade produced an assignment.\n");
        return EXIT_FAILURE;
    }
    if (!assignmentsFollowRosterRowOrder())
    {
        std::fprintf(stderr, "Assignments did not follow roster row order.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

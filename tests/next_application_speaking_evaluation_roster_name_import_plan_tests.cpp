#include "next/application/speaking_evaluation_roster_name_import_plan.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::
    SpeakingEvaluationRosterNameEvaluationRow;
using ClassMngr::Next::Application::SpeakingEvaluationRosterNamePair;
using ClassMngr::Next::Application::
    planSpeakingEvaluationRosterNameImport;

namespace
{

SpeakingEvaluationRosterNamePair names(
    std::u16string englishName,
    std::u16string koreanName
    )
{
    return {
        std::move(englishName),
        std::move(koreanName)
    };
}

SpeakingEvaluationRosterNameEvaluationRow evaluationRow(
    const int row,
    std::u16string englishName,
    std::u16string koreanName
    )
{
    return {
        row,
        names(std::move(englishName), std::move(koreanName))
    };
}

bool trimsCompletePairsAndSkipsEmptyOrIncompleteNames()
{
    const auto assignments = planSpeakingEvaluationRosterNameImport(
        {
            names(u" \t", u"\uAE40\uBBFC\uC9C0"),
            names(u"Alex", u"\u3000\u00a0"),
            names(u"\u00a0 Alex \u3000", u" \uAE40\uBBFC\uC9C0\t")
        },
        {evaluationRow(6, u"\u00a0\u3000", u" \t")}
        );

    return assignments.size() == 1
        && assignments[0].sourceRow == 2
        && assignments[0].targetRow == 6
        && assignments[0].names.englishName == u"Alex"
        && assignments[0].names.koreanName == u"\uAE40\uBBFC\uC9C0";
}

bool filtersExistingAndImportedPairsWhilePreservingSourceOrder()
{
    const auto assignments = planSpeakingEvaluationRosterNameImport(
        {
            names(u"Alice", u"\uAE40\uBBFC\uC9C0"),
            names(u" Bob ", u"\uAE40\uBCF4\uB78C "),
            names(u"Bob", u"\uAE40\uBCF4\uB78C"),
            names(u"bob", u"\uAE40\uBCF4\uB78C"),
            names(u"Carol", u"\uC774\uC608\uC740")
        },
        {
            evaluationRow(2, u" Alice ", u"\uAE40\uBBFC\uC9C0 "),
            evaluationRow(5, u"", u""),
            evaluationRow(8, u"", u"")
        }
        );

    return assignments.size() == 2
        && assignments[0].sourceRow == 1
        && assignments[0].targetRow == 5
        && assignments[0].names.englishName == u"Bob"
        && assignments[1].sourceRow == 3
        && assignments[1].targetRow == 8
        && assignments[1].names.englishName == u"bob";
}

bool targetRowsRequireBothNamesBlankAndAreConsumedInRowOrder()
{
    const auto assignments = planSpeakingEvaluationRosterNameImport(
        {
            names(u"First", u"\uAE40\uBBFC\uC9C0"),
            names(u"Second", u"\uC774\uC608\uC740")
        },
        {
            evaluationRow(1, u"", u"\uAE40\uBBFC\uC9C0"),
            evaluationRow(3, u"Existing", u""),
            evaluationRow(7, u"\u00a0 \u3000", u"\t"),
            evaluationRow(9, u"", u"")
        }
        );

    return assignments.size() == 2
        && assignments[0].sourceRow == 0
        && assignments[0].targetRow == 7
        && assignments[1].sourceRow == 1
        && assignments[1].targetRow == 9;
}

bool legacyDelimiterCollisionStillFiltersThePair()
{
    const auto assignments = planSpeakingEvaluationRosterNameImport(
        {names(u"A", u"B\u001fC")},
        {
            evaluationRow(1, u"A\u001fB", u"C"),
            evaluationRow(4, u"", u"")
        }
        );

    return assignments.empty();
}

bool noRosterOrNoBlankTargetProducesNoAssignments()
{
    return planSpeakingEvaluationRosterNameImport(
               {},
               {evaluationRow(0, u"", u"")}
               ).empty()
        && planSpeakingEvaluationRosterNameImport(
               {names(u"Alex", u"\uAE40\uBBFC\uC9C0")},
               {evaluationRow(0, u"Already", u"\uC774\uC608\uC740")}
               ).empty();
}

} // namespace

int main()
{
    if (!trimsCompletePairsAndSkipsEmptyOrIncompleteNames())
    {
        std::fprintf(stderr, "Incomplete or whitespace-only pairs were not filtered correctly.\n");
        return EXIT_FAILURE;
    }
    if (!filtersExistingAndImportedPairsWhilePreservingSourceOrder())
    {
        std::fprintf(stderr, "Existing/imported pair filtering or source order changed.\n");
        return EXIT_FAILURE;
    }
    if (!targetRowsRequireBothNamesBlankAndAreConsumedInRowOrder())
    {
        std::fprintf(stderr, "Target row blank rules or consumption order changed.\n");
        return EXIT_FAILURE;
    }
    if (!legacyDelimiterCollisionStillFiltersThePair())
    {
        std::fprintf(stderr, "Legacy U+001F delimiter collision behavior changed.\n");
        return EXIT_FAILURE;
    }
    if (!noRosterOrNoBlankTargetProducesNoAssignments())
    {
        std::fprintf(stderr, "Empty input unexpectedly produced an assignment.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

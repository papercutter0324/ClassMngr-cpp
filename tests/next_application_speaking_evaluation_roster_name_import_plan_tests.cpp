#include "next/application/speaking_evaluation_roster_name_import_plan.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::
    SpeakingEvaluationRosterNameImportSourceIndex;
using ClassMngr::Next::Application::
    SpeakingEvaluationRosterNameImportTargetIndex;
using ClassMngr::Next::Application::
    SpeakingEvaluationRosterNameImportAssignment;
using ClassMngr::Next::Application::
    SpeakingEvaluationRosterNameEvaluationRow;
using ClassMngr::Next::Application::SpeakingEvaluationRosterNamePair;
using ClassMngr::Next::Application::
    planSpeakingEvaluationRosterNameImport;

static_assert(
    std::is_same_v<
        decltype(SpeakingEvaluationRosterNameImportSourceIndex{}.rawValue()),
        int
        >
    );
static_assert(
    std::is_same_v<
        decltype(SpeakingEvaluationRosterNameImportTargetIndex{}.rawValue()),
        int
        >
    );
static_assert(
    std::is_same_v<
        decltype(SpeakingEvaluationRosterNameEvaluationRow::row),
        SpeakingEvaluationRosterNameImportTargetIndex
        >
    );
static_assert(
    std::is_same_v<
        decltype(SpeakingEvaluationRosterNameImportAssignment::sourceRow),
        SpeakingEvaluationRosterNameImportSourceIndex
        >
    );
static_assert(
    std::is_same_v<
        decltype(SpeakingEvaluationRosterNameImportAssignment::targetRow),
        SpeakingEvaluationRosterNameImportTargetIndex
        >
    );
static_assert(
    !std::is_convertible_v<SpeakingEvaluationRosterNameImportSourceIndex, int>
    );
static_assert(
    !std::is_convertible_v<int, SpeakingEvaluationRosterNameImportSourceIndex>
    );
static_assert(
    !std::is_convertible_v<SpeakingEvaluationRosterNameImportTargetIndex, int>
    );
static_assert(
    !std::is_convertible_v<int, SpeakingEvaluationRosterNameImportTargetIndex>
    );
static_assert(
    !std::is_convertible_v<
        SpeakingEvaluationRosterNameImportSourceIndex,
        SpeakingEvaluationRosterNameImportTargetIndex
        >
    );
static_assert(
    !std::is_convertible_v<
        SpeakingEvaluationRosterNameImportTargetIndex,
        SpeakingEvaluationRosterNameImportSourceIndex
        >
    );

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
    const SpeakingEvaluationRosterNameImportTargetIndex row,
    std::u16string englishName,
    std::u16string koreanName
    )
{
    return {
        row,
        names(std::move(englishName), std::move(koreanName))
    };
}

bool indexWrappersHaveExpectedDefaultsAndValues()
{
    const SpeakingEvaluationRosterNameImportSourceIndex defaultSource;
    const SpeakingEvaluationRosterNameImportTargetIndex defaultTarget;
    const SpeakingEvaluationRosterNameImportSourceIndex source{4};
    const SpeakingEvaluationRosterNameImportTargetIndex target{9};

    return defaultSource.rawValue() == -1
        && defaultTarget.rawValue() == -1
        && source.rawValue() == 4
        && target.rawValue() == 9
        && source == SpeakingEvaluationRosterNameImportSourceIndex{4}
        && target == SpeakingEvaluationRosterNameImportTargetIndex{9};
}

bool trimsCompletePairsAndSkipsEmptyOrIncompleteNames()
{
    const auto assignments = planSpeakingEvaluationRosterNameImport(
        {
            names(u" \t", u"\uAE40\uBBFC\uC9C0"),
            names(u"Alex", u"\u3000\u00a0"),
            names(u"\u00a0 Alex \u3000", u" \uAE40\uBBFC\uC9C0\t")
        },
        {
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{6},
                u"\u00a0\u3000",
                u" \t"
                )
        }
        );

    return assignments.size() == 1
        && assignments[0].sourceRow
            == SpeakingEvaluationRosterNameImportSourceIndex{2}
        && assignments[0].targetRow
            == SpeakingEvaluationRosterNameImportTargetIndex{6}
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
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{2},
                u" Alice ",
                u"\uAE40\uBBFC\uC9C0 "
                ),
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{5},
                u"",
                u""
                ),
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{8},
                u"",
                u""
                )
        }
        );

    return assignments.size() == 2
        && assignments[0].sourceRow
            == SpeakingEvaluationRosterNameImportSourceIndex{1}
        && assignments[0].targetRow
            == SpeakingEvaluationRosterNameImportTargetIndex{5}
        && assignments[0].names.englishName == u"Bob"
        && assignments[1].sourceRow
            == SpeakingEvaluationRosterNameImportSourceIndex{3}
        && assignments[1].targetRow
            == SpeakingEvaluationRosterNameImportTargetIndex{8}
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
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{1},
                u"",
                u"\uAE40\uBBFC\uC9C0"
                ),
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{3},
                u"Existing",
                u""
                ),
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{7},
                u"\u00a0 \u3000",
                u"\t"
                ),
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{9},
                u"",
                u""
                )
        }
        );

    return assignments.size() == 2
        && assignments[0].sourceRow
            == SpeakingEvaluationRosterNameImportSourceIndex{0}
        && assignments[0].targetRow
            == SpeakingEvaluationRosterNameImportTargetIndex{7}
        && assignments[1].sourceRow
            == SpeakingEvaluationRosterNameImportSourceIndex{1}
        && assignments[1].targetRow
            == SpeakingEvaluationRosterNameImportTargetIndex{9};
}

bool targetValuesPreserveEvaluationInputOrder()
{
    const auto assignments = planSpeakingEvaluationRosterNameImport(
        {
            names(u"First", u"\uAE40\uBBFC\uC9C0"),
            names(u"Second", u"\uC774\uC608\uC740")
        },
        {
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{12},
                u"",
                u""
                ),
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{4},
                u"",
                u""
                )
        }
        );

    return assignments.size() == 2
        && assignments[0].targetRow
            == SpeakingEvaluationRosterNameImportTargetIndex{12}
        && assignments[0].names.englishName == u"First"
        && assignments[1].targetRow
            == SpeakingEvaluationRosterNameImportTargetIndex{4}
        && assignments[1].names.englishName == u"Second";
}

bool legacyDelimiterCollisionStillFiltersThePair()
{
    const auto assignments = planSpeakingEvaluationRosterNameImport(
        {names(u"A", u"B\u001fC")},
        {
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{1},
                u"A\u001fB",
                u"C"
                ),
            evaluationRow(
                SpeakingEvaluationRosterNameImportTargetIndex{4},
                u"",
                u""
                )
        }
        );

    return assignments.empty();
}

bool noRosterOrNoBlankTargetProducesNoAssignments()
{
    return planSpeakingEvaluationRosterNameImport(
               {},
               {
                   evaluationRow(
                       SpeakingEvaluationRosterNameImportTargetIndex{0},
                       u"",
                       u""
                       )
               }
               ).empty()
        && planSpeakingEvaluationRosterNameImport(
               {names(u"Alex", u"\uAE40\uBBFC\uC9C0")},
               {
                   evaluationRow(
                       SpeakingEvaluationRosterNameImportTargetIndex{0},
                       u"Already",
                       u"\uC774\uC608\uC740"
                       )
               }
               ).empty();
}

} // namespace

int main()
{
    if (!indexWrappersHaveExpectedDefaultsAndValues())
    {
        std::fprintf(stderr, "Index wrappers did not preserve their default or supplied values.\n");
        return EXIT_FAILURE;
    }
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
    if (!targetValuesPreserveEvaluationInputOrder())
    {
        std::fprintf(stderr, "Caller-supplied target index order was not preserved.\n");
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

#include "next/application/class_analytics_criterion_extrema_policy.h"

#include <cassert>
#include <cstddef>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

struct CriterionFacts final
{
    bool hasData = false;
    double average3 = 0.0;
};

void strongestAndFocusTiesPreserveInputOrderAndSkipMissing()
{
    const std::vector<CriterionFacts> criteria{
        { true, 4.0 },
        { false, 5.0 },
        { true, 1.0 },
        { true, 4.0 },
        { false, -1.0 },
        { true, 1.0 }
    };

    const auto selection = selectClassAnalyticsCriterionExtrema(criteria);
    assert((selection.strongestIndices == std::vector<std::size_t>{ 0, 3 }));
    assert((selection.focusIndices == std::vector<std::size_t>{ 2, 5 }));
}

void emptyAndAllMissingInputsHaveNoSelections()
{
    const std::vector<CriterionFacts> empty;
    const auto emptySelection = selectClassAnalyticsCriterionExtrema(empty);
    assert(emptySelection.strongestIndices.empty());
    assert(emptySelection.focusIndices.empty());

    const std::vector<CriterionFacts> missing{
        { false, -1.0 },
        { false, 5.0 }
    };
    const auto missingSelection = selectClassAnalyticsCriterionExtrema(missing);
    assert(missingSelection.strongestIndices.empty());
    assert(missingSelection.focusIndices.empty());
}

void adjacentThousandthsRemainDistinct()
{
    const std::vector<CriterionFacts> criteria{
        { true, 3.0 },
        { true, 3.001 }
    };

    const auto selection = selectClassAnalyticsCriterionExtrema(criteria);
    assert((selection.strongestIndices == std::vector<std::size_t>{ 1 }));
    assert((selection.focusIndices == std::vector<std::size_t>{ 0 }));
}

void strictToleranceKeepsOnlyNearTies()
{
    const std::vector<CriterionFacts> criteria{
        { true, 2.0 },
        { true, 2.0000000005 },
        { true, 2.001 }
    };

    const auto selection = selectClassAnalyticsCriterionExtrema(criteria);
    assert((selection.focusIndices == std::vector<std::size_t>{ 0, 1 }));
    assert((selection.strongestIndices == std::vector<std::size_t>{ 2 }));
}

void strictToleranceExcludesTheExactBoundary()
{
    const std::vector<CriterionFacts> criteria{
        { true, 0.0 },
        { true, 1e-9 }
    };

    const auto selection = selectClassAnalyticsCriterionExtrema(criteria);
    assert((selection.focusIndices == std::vector<std::size_t>{ 0 }));
    assert((selection.strongestIndices == std::vector<std::size_t>{ 1 }));
}

} // namespace

int main()
{
    strongestAndFocusTiesPreserveInputOrderAndSkipMissing();
    emptyAndAllMissingInputsHaveNoSelections();
    adjacentThousandthsRemainDistinct();
    strictToleranceKeepsOnlyNearTies();
    strictToleranceExcludesTheExactBoundary();
    return 0;
}

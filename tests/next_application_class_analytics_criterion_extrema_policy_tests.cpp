#include "next/application/class_analytics_criterion_extrema_policy.h"
#include "next/application/class_analytics_average_thousandths.h"

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

struct CriterionFacts final
{
    bool hasData = false;
    double average3 = 0.0;
};

struct ThousandthsCriterionFacts final
{
    bool hasData = false;
    ClassAnalyticsAverageThousandths average3;
};

std::vector<ClassAnalyticsCriterionIndex> criterionIndices(
    const std::initializer_list<std::size_t> rawIndices
)
{
    std::vector<ClassAnalyticsCriterionIndex> indices;
    indices.reserve(rawIndices.size());
    for (const std::size_t rawIndex : rawIndices)
    {
        indices.emplace_back(rawIndex);
    }
    return indices;
}

void criterionIndexHasExplicitValueSemantics()
{
    using Index = ClassAnalyticsCriterionIndex;

    static_assert(std::is_default_constructible_v<Index>);
    static_assert(std::is_constructible_v<Index, std::size_t>);
    static_assert(!std::is_convertible_v<std::size_t, Index>);
    static_assert(!std::is_convertible_v<Index, std::size_t>);
    static_assert(std::is_same_v<
        decltype(ClassAnalyticsCriterionExtremaSelection::strongestIndices),
        std::vector<Index>>);
    static_assert(std::is_same_v<
        decltype(ClassAnalyticsCriterionExtremaSelection::focusIndices),
        std::vector<Index>>);

    const Index defaultIndex;
    const Index explicitIndex{ 7 };
    assert(defaultIndex.rawValue() == 0);
    assert(explicitIndex.rawValue() == 7);
    assert(Index{ 7 } == explicitIndex);
    assert(Index{ 8 } != explicitIndex);
}

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
    assert(selection.strongestIndices == criterionIndices({ 0, 3 }));
    assert(selection.focusIndices == criterionIndices({ 2, 5 }));
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
    assert(selection.strongestIndices == criterionIndices({ 1 }));
    assert(selection.focusIndices == criterionIndices({ 0 }));
}

void equalThousandthsPreserveCriterionOrder()
{
    const std::vector<ThousandthsCriterionFacts> criteria{
        { true, ClassAnalyticsAverageThousandths{ 3001 } },
        { true, ClassAnalyticsAverageThousandths{ 2399 } },
        { true, ClassAnalyticsAverageThousandths{ 3001 } },
        { true, ClassAnalyticsAverageThousandths{ 2399 } }
    };

    const auto selection = selectClassAnalyticsCriterionExtrema(criteria);
    assert(selection.strongestIndices == criterionIndices({ 0, 2 }));
    assert(selection.focusIndices == criterionIndices({ 1, 3 }));
}

void strictToleranceKeepsOnlyNearTies()
{
    const std::vector<CriterionFacts> criteria{
        { true, 2.0 },
        { true, 2.0000000005 },
        { true, 2.001 }
    };

    const auto selection = selectClassAnalyticsCriterionExtrema(criteria);
    assert(selection.focusIndices == criterionIndices({ 0, 1 }));
    assert(selection.strongestIndices == criterionIndices({ 2 }));
}

void strictToleranceExcludesTheExactBoundary()
{
    const std::vector<CriterionFacts> criteria{
        { true, 0.0 },
        { true, 1e-9 }
    };

    const auto selection = selectClassAnalyticsCriterionExtrema(criteria);
    assert(selection.focusIndices == criterionIndices({ 0 }));
    assert(selection.strongestIndices == criterionIndices({ 1 }));
}

} // namespace

int main()
{
    criterionIndexHasExplicitValueSemantics();
    strongestAndFocusTiesPreserveInputOrderAndSkipMissing();
    emptyAndAllMissingInputsHaveNoSelections();
    adjacentThousandthsRemainDistinct();
    equalThousandthsPreserveCriterionOrder();
    strictToleranceKeepsOnlyNearTies();
    strictToleranceExcludesTheExactBoundary();
    return 0;
}

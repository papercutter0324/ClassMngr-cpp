#include "next/application/evaluation_default_selection.h"

#include <array>
#include <cstdlib>

using ClassMngr::Next::Application::EvaluationDefaultPolicy;
using ClassMngr::Next::Application::EvaluationPeriod;
using ClassMngr::Next::Application::selectDefaultEvaluationPeriod;

namespace
{

struct CycleCase
{
    EvaluationPeriod current;
    EvaluationPeriod previous;
};

constexpr std::array<CycleCase, 4> CycleCases{{
    {EvaluationPeriod::Winter, EvaluationPeriod::Fall},
    {EvaluationPeriod::Spring, EvaluationPeriod::Winter},
    {EvaluationPeriod::Summer, EvaluationPeriod::Spring},
    {EvaluationPeriod::Fall, EvaluationPeriod::Summer}
}};

} // namespace

int main()
{
    for (const CycleCase& cycleCase : CycleCases)
    {
        if (selectDefaultEvaluationPeriod(
                EvaluationDefaultPolicy::CurrentOrPreviousTerm,
                cycleCase.current,
                true
                ) != cycleCase.current)
        {
            return EXIT_FAILURE;
        }

        if (selectDefaultEvaluationPeriod(
                EvaluationDefaultPolicy::CurrentOrPreviousTerm,
                cycleCase.current,
                false
                ) != cycleCase.previous)
        {
            return EXIT_FAILURE;
        }

        if (selectDefaultEvaluationPeriod(
                EvaluationDefaultPolicy::All,
                cycleCase.current,
                true
                ).has_value()
            || selectDefaultEvaluationPeriod(
                   EvaluationDefaultPolicy::All,
                   cycleCase.current,
                   false
                   ).has_value())
        {
            return EXIT_FAILURE;
        }
    }

    constexpr auto InvalidPeriod = static_cast<EvaluationPeriod>(255);
    if (selectDefaultEvaluationPeriod(
            EvaluationDefaultPolicy::CurrentOrPreviousTerm,
            InvalidPeriod,
            true
            ).has_value()
        || selectDefaultEvaluationPeriod(
               EvaluationDefaultPolicy::CurrentOrPreviousTerm,
               InvalidPeriod,
               false
               ).has_value())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

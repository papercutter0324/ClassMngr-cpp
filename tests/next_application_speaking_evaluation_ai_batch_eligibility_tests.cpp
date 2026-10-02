#include "next/application/speaking_evaluation_ai_batch_eligibility.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchEligibilityInput;
using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchEligibilityReason;
using ClassMngr::Next::Application::
    speakingEvaluationAiBatchEligibilityReason;

namespace
{

bool reportsMissingNameBeforeOtherFailures()
{
    return speakingEvaluationAiBatchEligibilityReason({
        .hasTrimmedStudentName = false,
        .grade = 0,
        .hasDidWellItem = false,
        .hasNeedsImprovementItem = false
    }) == SpeakingEvaluationAiBatchEligibilityReason::MissingName;
}

bool reportsUnsupportedGradeBeforeMissingObservations()
{
    const SpeakingEvaluationAiBatchEligibilityInput input{
        .hasTrimmedStudentName = true,
        .grade = 7,
        .hasDidWellItem = false,
        .hasNeedsImprovementItem = false
    };
    return speakingEvaluationAiBatchEligibilityReason(input)
        == SpeakingEvaluationAiBatchEligibilityReason::UnsupportedGrade;
}

bool reportsDidWellBeforeNeedsImprovement()
{
    const SpeakingEvaluationAiBatchEligibilityInput input{
        .hasTrimmedStudentName = true,
        .grade = 5,
        .hasDidWellItem = false,
        .hasNeedsImprovementItem = false
    };
    return speakingEvaluationAiBatchEligibilityReason(input)
        == SpeakingEvaluationAiBatchEligibilityReason::MissingDidWellItem;
}

bool reportsMissingNeedsImprovementAfterDidWellExists()
{
    const SpeakingEvaluationAiBatchEligibilityInput input{
        .hasTrimmedStudentName = true,
        .grade = 5,
        .hasDidWellItem = true,
        .hasNeedsImprovementItem = false
    };
    return speakingEvaluationAiBatchEligibilityReason(input)
        == SpeakingEvaluationAiBatchEligibilityReason::
            MissingNeedsImprovementItem;
}

bool acceptsSupportedGradesWhenAllInputsExist()
{
    for (const int grade : {4, 5, 6})
    {
        const SpeakingEvaluationAiBatchEligibilityInput input{
            .hasTrimmedStudentName = true,
            .grade = grade,
            .hasDidWellItem = true,
            .hasNeedsImprovementItem = true
        };
        if (speakingEvaluationAiBatchEligibilityReason(input)
            != SpeakingEvaluationAiBatchEligibilityReason::Eligible)
        {
            return false;
        }
    }
    return true;
}

bool rejectsGradesOutsideSupportedRange()
{
    for (const int grade : {3, 7})
    {
        const SpeakingEvaluationAiBatchEligibilityInput input{
            .hasTrimmedStudentName = true,
            .grade = grade,
            .hasDidWellItem = true,
            .hasNeedsImprovementItem = true
        };
        if (speakingEvaluationAiBatchEligibilityReason(input)
            != SpeakingEvaluationAiBatchEligibilityReason::UnsupportedGrade)
        {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    if (!reportsMissingNameBeforeOtherFailures())
    {
        std::fprintf(stderr, "Missing name did not take first-failure precedence.\n");
        return EXIT_FAILURE;
    }
    if (!reportsUnsupportedGradeBeforeMissingObservations())
    {
        std::fprintf(stderr, "Unsupported grade did not precede observation failures.\n");
        return EXIT_FAILURE;
    }
    if (!reportsDidWellBeforeNeedsImprovement())
    {
        std::fprintf(stderr, "Missing Did Well item did not take precedence.\n");
        return EXIT_FAILURE;
    }
    if (!reportsMissingNeedsImprovementAfterDidWellExists())
    {
        std::fprintf(stderr, "Missing Needs Improvement item was not reported.\n");
        return EXIT_FAILURE;
    }
    if (!acceptsSupportedGradesWhenAllInputsExist())
    {
        std::fprintf(stderr, "An eligible supported-grade input was rejected.\n");
        return EXIT_FAILURE;
    }
    if (!rejectsGradesOutsideSupportedRange())
    {
        std::fprintf(stderr, "A grade outside E4 through E6 was accepted.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#include "next/application/speaking_evaluation_ai_batch_eligibility.h"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchEligibilityInput;
using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchEligibilityReason;
using ClassMngr::Next::Application::
    speakingEvaluationAiBatchEligibilityReason;
using ClassMngr::Next::Application::
    speakingEvaluationCourseGradeBandFromInteger;
using ClassMngr::Next::Domain::CourseGradeBand;

namespace
{

bool reportsMissingNameBeforeOtherFailures()
{
    return speakingEvaluationAiBatchEligibilityReason({
        .hasTrimmedStudentName = false,
        .grade = CourseGradeBand::Other,
        .hasDidWellItem = false,
        .hasNeedsImprovementItem = false
    }) == SpeakingEvaluationAiBatchEligibilityReason::MissingName;
}

bool reportsUnsupportedGradeBeforeMissingObservations()
{
    const SpeakingEvaluationAiBatchEligibilityInput input{
        .hasTrimmedStudentName = true,
        .grade = CourseGradeBand::Other,
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
        .grade = CourseGradeBand::E5,
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
        .grade = CourseGradeBand::E5,
        .hasDidWellItem = true,
        .hasNeedsImprovementItem = false
    };
    return speakingEvaluationAiBatchEligibilityReason(input)
        == SpeakingEvaluationAiBatchEligibilityReason::
            MissingNeedsImprovementItem;
}

bool acceptsSupportedGradesWhenAllInputsExist()
{
    for (const CourseGradeBand grade : {
             CourseGradeBand::E4,
             CourseGradeBand::E5,
             CourseGradeBand::E6
         })
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

bool rejectsOtherAndMiddleSchoolBands()
{
    for (const CourseGradeBand grade : {
             CourseGradeBand::Other,
             CourseGradeBand::M1,
             CourseGradeBand::M2,
             CourseGradeBand::M3
         })
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

bool integerBoundaryMapsOnlyElementaryGradesToBands()
{
    const std::vector<std::pair<int, CourseGradeBand>> cases{
        {4, CourseGradeBand::E4},
        {5, CourseGradeBand::E5},
        {6, CourseGradeBand::E6},
        {-1, CourseGradeBand::Other},
        {0, CourseGradeBand::Other},
        {3, CourseGradeBand::Other},
        {7, CourseGradeBand::Other},
        {std::numeric_limits<int>::min(), CourseGradeBand::Other},
        {std::numeric_limits<int>::max(), CourseGradeBand::Other}
    };
    for (const auto& [grade, expected] : cases)
    {
        if (speakingEvaluationCourseGradeBandFromInteger(grade) != expected)
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
    if (!rejectsOtherAndMiddleSchoolBands())
    {
        std::fprintf(stderr, "An unsupported grade band was accepted.\n");
        return EXIT_FAILURE;
    }
    if (!integerBoundaryMapsOnlyElementaryGradesToBands())
    {
        std::fprintf(stderr, "Integer-to-grade-band mapping was incorrect.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

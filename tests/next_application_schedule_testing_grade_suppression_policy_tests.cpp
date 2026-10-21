#include "next/application/schedule_testing_grade_suppression_policy.h"

#include <array>
#include <cstdlib>

int main()
{
    using ClassMngr::Next::Application::
        scheduleTestingSuppressesGradeBand;
    using ClassMngr::Next::Domain::CourseGradeBand;

    struct ExpectedResult
    {
        CourseGradeBand gradeBand;
        bool testingAffectsM1;
        bool suppressed;
    };

    constexpr std::array cases{
        ExpectedResult{CourseGradeBand::E4, false, false},
        ExpectedResult{CourseGradeBand::E4, true, false},
        ExpectedResult{CourseGradeBand::E5, false, false},
        ExpectedResult{CourseGradeBand::E5, true, false},
        ExpectedResult{CourseGradeBand::E6, false, false},
        ExpectedResult{CourseGradeBand::E6, true, false},
        ExpectedResult{CourseGradeBand::M1, false, false},
        ExpectedResult{CourseGradeBand::M1, true, true},
        ExpectedResult{CourseGradeBand::M2, false, true},
        ExpectedResult{CourseGradeBand::M2, true, true},
        ExpectedResult{CourseGradeBand::M3, false, true},
        ExpectedResult{CourseGradeBand::M3, true, true},
        ExpectedResult{CourseGradeBand::Other, false, false},
        ExpectedResult{CourseGradeBand::Other, true, false}
    };

    for (const ExpectedResult& testCase : cases)
    {
        if (scheduleTestingSuppressesGradeBand(
                testCase.gradeBand,
                testCase.testingAffectsM1
                ) != testCase.suppressed)
        {
            return EXIT_FAILURE;
        }
    }

    const auto unknownGradeBand = static_cast<CourseGradeBand>(-1);
    if (scheduleTestingSuppressesGradeBand(unknownGradeBand, false)
        || scheduleTestingSuppressesGradeBand(unknownGradeBand, true))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

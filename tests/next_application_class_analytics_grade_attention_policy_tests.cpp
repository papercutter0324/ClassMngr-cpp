#include "next/application/class_analytics_grade_attention_policy.h"

#include <array>
#include <cstdlib>
#include <utility>

using ClassMngr::Next::Application::classAnalyticsGradeNeedsAttention;
using ClassMngr::Next::Domain::SpeakingEvaluationGrade;

int main()
{
    constexpr std::array cases{
        std::pair{SpeakingEvaluationGrade::C, true},
        std::pair{SpeakingEvaluationGrade::B, true},
        std::pair{SpeakingEvaluationGrade::BPlus, false},
        std::pair{SpeakingEvaluationGrade::A, false},
        std::pair{SpeakingEvaluationGrade::APlus, false}
    };

    for (const auto& [grade, expected] : cases)
    {
        if (classAnalyticsGradeNeedsAttention(grade) != expected)
        {
            return EXIT_FAILURE;
        }
    }

    if (!classAnalyticsGradeNeedsAttention(
            static_cast<SpeakingEvaluationGrade>(0)
            ))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

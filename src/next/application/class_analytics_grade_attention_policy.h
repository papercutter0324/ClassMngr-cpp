#pragma once

#include "next/domain/speaking_evaluation_grade.h"

namespace ClassMngr::Next::Application
{

[[nodiscard]] constexpr bool classAnalyticsGradeNeedsAttention(
    Domain::SpeakingEvaluationGrade grade
    ) noexcept
{
    switch (grade)
    {
    case Domain::SpeakingEvaluationGrade::C:
    case Domain::SpeakingEvaluationGrade::B:
        return true;
    case Domain::SpeakingEvaluationGrade::BPlus:
    case Domain::SpeakingEvaluationGrade::A:
    case Domain::SpeakingEvaluationGrade::APlus:
        return false;
    }

    return true;
}

} // namespace ClassMngr::Next::Application

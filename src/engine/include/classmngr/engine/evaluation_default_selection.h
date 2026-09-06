#pragma once

#include "classmngr/engine/academic_calendar.h"
#include "classmngr/engine/speaking_evaluation.h"

#include <string>
#include <string_view>

namespace classmngr::engine
{

class EvaluationDefaultSelection final
{
public:
    [[nodiscard]] static std::string evaluationNameForTerm(
        AcademicTerm term
        );

    // Row values are expected to be UTF-8. Only standard ASCII whitespace is
    // treated as blank at this portable boundary.
    [[nodiscard]] static bool isPopulated(
        const SpeakingEvaluationRows& rows
        );

    [[nodiscard]] static AcademicTerm termForEvaluation(
        AcademicTerm currentTerm,
        bool currentEvaluationIsPopulated
        ) noexcept;

    [[nodiscard]] static SchoolLevel schoolLevelForGrade(
        std::string_view grade
        ) noexcept;
};

} // namespace classmngr::engine

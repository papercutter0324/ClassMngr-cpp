#include "classmngr/engine/evaluation_default_selection.h"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace classmngr::engine;

namespace
{
bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineEvaluationDefaultSelectionTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    bool passed = true;

    const std::array<std::pair<AcademicTerm, std::string_view>, 4> names{
        std::pair{AcademicTerm::Winter, "Winter"},
        std::pair{AcademicTerm::Spring, "Speech Contest"},
        std::pair{AcademicTerm::Summer, "Summer"},
        std::pair{AcademicTerm::Fall, "Fall"}
    };
    for (const auto& [term, expected] : names)
    {
        passed &= expect(
            EvaluationDefaultSelection::evaluationNameForTerm(term)
                == expected,
            "term name mapping changed"
            );
    }

    for (const auto& entry : names)
    {
        const AcademicTerm term = entry.first;
        passed &= expect(
            EvaluationDefaultSelection::termForEvaluation(term, true) == term,
            "populated current term was not retained"
            );

        const AcademicTerm previous =
            term == AcademicTerm::Winter
                ? AcademicTerm::Fall
                : term == AcademicTerm::Spring
                    ? AcademicTerm::Winter
                    : term == AcademicTerm::Summer
                        ? AcademicTerm::Spring
                        : AcademicTerm::Summer;
        passed &= expect(
            EvaluationDefaultSelection::termForEvaluation(term, false)
                == previous,
            "empty current term did not fall back to the previous term"
            );
    }

    passed &= expect(
        !EvaluationDefaultSelection::isPopulated({}),
        "empty rows were reported as populated"
        );
    passed &= expect(
        !EvaluationDefaultSelection::isPopulated({
            SpeakingEvaluationRow{" \t\n\r\f\v"},
            SpeakingEvaluationRow{}
        }),
        "ASCII whitespace was reported as content"
        );
    passed &= expect(
        EvaluationDefaultSelection::isPopulated({
            SpeakingEvaluationRow{" \t"},
            SpeakingEvaluationRow{" \xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98 "}
        }),
        "UTF-8 evaluation content was not recognized"
        );

    const std::array<std::pair<std::string_view, SchoolLevel>, 7> grades{
        std::pair{"M1", SchoolLevel::Middle},
        std::pair{" m2 ", SchoolLevel::Middle},
        std::pair{"m3", SchoolLevel::Middle},
        std::pair{"E1", SchoolLevel::Elementary},
        std::pair{"M4", SchoolLevel::Elementary},
        std::pair{"", SchoolLevel::Elementary},
        std::pair{"\xea\xb9\x80\xeb\xaf\xbc\xec\x88\x98", SchoolLevel::Elementary}
    };
    for (const auto& [grade, expected] : grades)
    {
        passed &= expect(
            EvaluationDefaultSelection::schoolLevelForGrade(grade)
                == expected,
            "grade school-level classification changed"
            );
    }

    return passed ? 0 : 1;
}

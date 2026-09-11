#include "classmngr/engine/speaking_evaluation_report_service.h"

#include <array>
#include <string_view>

namespace classmngr::engine
{
namespace
{
struct GradeValue
{
    std::string_view label;
    int value;
};

constexpr std::array<GradeValue, 5> gradeValues{
    GradeValue{"C", 1},
    GradeValue{"B", 2},
    GradeValue{"B+", 3},
    GradeValue{"A", 4},
    GradeValue{"A+", 5}
};

int gradeValue(std::string_view grade)
{
    for (const GradeValue& candidate : gradeValues)
    {
        if (candidate.label == grade)
        {
            return candidate.value;
        }
    }

    return 0;
}
} // namespace

std::string SpeakingEvaluationReportService::overallGrade(
    const SpeakingEvaluationScores& scores
    )
{
    int sum = 0;
    for (const std::string& score : scores)
    {
        const int value = gradeValue(score);
        if (value == 0)
        {
            return "N/A";
        }

        sum += value;
    }

    // The retained Qt implementation rounds up only when the fractional
    // average is at least 0.4.  Compare integer quantities to keep that rule
    // deterministic and avoid introducing floating-point behavior into the
    // portable engine.
    const int count = static_cast<int>(scores.size());
    int rounded = sum / count;
    if ((sum % count) * 10 >= count * 4)
    {
        ++rounded;
    }

    if (rounded < 1)
    {
        rounded = 1;
    }
    else if (rounded > 5)
    {
        rounded = 5;
    }

    return std::string(gradeValues.at(static_cast<std::size_t>(rounded - 1)).label);
}

} // namespace classmngr::engine

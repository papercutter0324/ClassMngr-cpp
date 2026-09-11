#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace classmngr::engine
{

inline constexpr std::size_t SpeakingEvaluationScoreCount = 6;
using SpeakingEvaluationScores =
    std::array<std::string, SpeakingEvaluationScoreCount>;

class SpeakingEvaluationReportService final
{
public:
    // Returns the persisted report grade, or "N/A" when any metric is not a
    // recognized speaking-evaluation grade.
    [[nodiscard]] static std::string overallGrade(
        const SpeakingEvaluationScores& scores
        );
};

} // namespace classmngr::engine

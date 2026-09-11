#include "classmngr/engine/speaking_evaluation_report_service.h"

#include <iostream>
#include <string>
#include <string_view>

namespace
{
using classmngr::engine::SpeakingEvaluationReportService;
using classmngr::engine::SpeakingEvaluationScores;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationReportServiceTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    bool passed = true;

    SpeakingEvaluationScores allAPlus;
    allAPlus.fill("A+");
    passed &= expect(
        SpeakingEvaluationReportService::overallGrade(allAPlus) == "A+",
        "the highest scores did not produce A+"
        );

    SpeakingEvaluationScores allC;
    allC.fill("C");
    passed &= expect(
        SpeakingEvaluationReportService::overallGrade(allC) == "C",
        "the lowest scores did not produce C"
        );

    passed &= expect(
        SpeakingEvaluationReportService::overallGrade({
            "A+", "A", "B+", "B", "A", "A+"
        }) == "A",
        "the retained 0.4 rounding threshold changed"
        );
    passed &= expect(
        SpeakingEvaluationReportService::overallGrade({
            "A+", "A+", "B+", "B", "B", "B"
        }) == "B+",
        "a fractional average below the threshold was rounded incorrectly"
        );
    passed &= expect(
        SpeakingEvaluationReportService::overallGrade({
            "A+", "A+", "A", "B+", "B+", "C"
        }) == "A",
        "a fractional average at or above the threshold was rounded incorrectly"
        );
    passed &= expect(
        SpeakingEvaluationReportService::overallGrade({
            "A+", "A", "B+", "B", "C", "invalid"
        }) == "N/A",
        "an invalid metric did not produce N/A"
        );

    return passed ? 0 : 1;
}

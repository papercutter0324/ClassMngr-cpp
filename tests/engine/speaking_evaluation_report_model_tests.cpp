#include "classmngr/engine/speaking_evaluation_report_model.h"

#include <iostream>
#include <string_view>

namespace
{
using classmngr::engine::SpeakingEvaluationReportModel;
using classmngr::engine::SpeakingEvaluationReportTemplate;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationReportModelTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        SpeakingEvaluationReportModel::elementaryGrade(" E4 ") == 4,
        "elementary grade normalization changed"
        );
    passed &= expect(
        SpeakingEvaluationReportModel::elementaryGrade("e6") == 6,
        "case-insensitive elementary grade parsing changed"
        );
    passed &= expect(
        SpeakingEvaluationReportModel::elementaryGrade("M1") == 0,
        "non-elementary grade was accepted"
        );

    passed &= expect(
        SpeakingEvaluationReportModel::classLabel(" E5 ", " Athena ")
            == "E5 Athena",
        "class label trimming or joining changed"
        );
    passed &= expect(
        SpeakingEvaluationReportModel::classLabel({}, "Athena") == "Athena",
        "class label did not preserve a level-only label"
        );

    passed &= expect(
        SpeakingEvaluationReportModel::templateForClass("e5", "ATHENA")
            == SpeakingEvaluationReportTemplate::Advanced,
        "E5 Athena did not select the advanced template"
        );
    passed &= expect(
        SpeakingEvaluationReportModel::templateForClass(" E6 ", " Song's ")
            == SpeakingEvaluationReportTemplate::Advanced,
        "E6 Song's did not select the advanced template"
        );
    passed &= expect(
        SpeakingEvaluationReportModel::templateForClass("E5", "Odysseus")
            == SpeakingEvaluationReportTemplate::Standard,
        "unrecognized class selected the advanced template"
        );

    passed &= expect(
        SpeakingEvaluationReportModel::reportDate(
            2026,
            5,
            SpeakingEvaluationReportTemplate::Standard
            ) == "May 2026",
        "standard May date formatting changed"
        );
    passed &= expect(
        SpeakingEvaluationReportModel::reportDate(
            2026,
            7,
            SpeakingEvaluationReportTemplate::Standard
            ) == "July 2026",
        "standard July date formatting changed"
        );
    passed &= expect(
        SpeakingEvaluationReportModel::reportDate(
            2026,
            7,
            SpeakingEvaluationReportTemplate::Advanced
            ) == "Jul. 2026",
        "advanced date formatting changed"
        );
    passed &= expect(
        SpeakingEvaluationReportModel::reportDate(
            2026,
            0,
            SpeakingEvaluationReportTemplate::Standard
            ).empty(),
        "invalid report month did not return an empty date"
        );

    return passed ? 0 : 1;
}

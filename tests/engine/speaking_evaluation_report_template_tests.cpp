#include "classmngr/engine/speaking_evaluation_report_template.h"

#include <iostream>
#include <string_view>

namespace
{
using classmngr::engine::SpeakingEvaluationReportTemplate;
using classmngr::engine::SpeakingEvaluationReportTemplatePolicy;
using classmngr::engine::SpeakingEvaluationReportTemplateService;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationReportTemplateTests: "
              << message
              << '\n';
    return false;
}

bool sameBounds(
    const classmngr::engine::SpeakingEvaluationReportRectangle& bounds,
    double left,
    double top,
    double width,
    double height
    )
{
    return bounds.left == left
        && bounds.top == top
        && bounds.width == width
        && bounds.height == height;
}

bool expectStandardPolicy(
    const SpeakingEvaluationReportTemplatePolicy& policy
    )
{
    bool passed = true;
    passed &= expect(
        policy.reportTemplate == SpeakingEvaluationReportTemplate::Standard,
        "standard policy reports the wrong template"
        );
    passed &= expect(
        policy.pageWidth == 540.0 && policy.pageHeight == 780.0,
        "standard page size changed"
        );
    passed &= expect(
        sameBounds(policy.signatureBounds, 374.0, 731.0, 131.0, 26.0),
        "standard signature bounds changed"
        );
    passed &= expect(
        policy.powerPointResourcePath
            == "Speaking Evaluations/SpeakingEvaluationTemplate-Full.pptx",
        "standard PowerPoint resource changed"
        );
    passed &= expect(
        !policy.usesAdvancedScoreTable
            && policy.signatureAlignsBottomLeft
            && policy.scoreTableOnMaster,
        "standard template flags changed"
        );
    passed &= expect(
        policy.scoreTableName == "Grades & Scores"
            && policy.minimumTableRows == 12
            && policy.minimumTableColumns == 6
            && policy.firstGradeColumn == 2,
        "standard score-table policy changed"
        );
    passed &= expect(
        policy.neutralFillRed == 217
            && policy.neutralFillGreen == 217
            && policy.neutralFillBlue == 217,
        "standard neutral fill changed"
        );
    return passed;
}

bool expectAdvancedPolicy(
    const SpeakingEvaluationReportTemplatePolicy& policy
    )
{
    bool passed = true;
    passed &= expect(
        policy.reportTemplate == SpeakingEvaluationReportTemplate::Advanced,
        "advanced policy reports the wrong template"
        );
    passed &= expect(
        policy.pageWidth == 540.0 && policy.pageHeight == 780.0,
        "advanced page size changed"
        );
    passed &= expect(
        sameBounds(policy.signatureBounds, 408.5, 746.25, 114.0, 24.75),
        "advanced signature bounds changed"
        );
    passed &= expect(
        policy.powerPointResourcePath
            == "Speaking Evaluations/SpeakingEvaluationTemplate_Advanced-Full.pptx",
        "advanced PowerPoint resource changed"
        );
    passed &= expect(
        policy.usesAdvancedScoreTable
            && policy.signatureAlignsBottomLeft
            && !policy.scoreTableOnMaster,
        "advanced template flags changed"
        );
    passed &= expect(
        policy.scoreTableName == "Report_Table"
            && policy.minimumTableRows == 12
            && policy.minimumTableColumns == 7
            && policy.firstGradeColumn == 3,
        "advanced score-table policy changed"
        );
    passed &= expect(
        policy.neutralFillRed == 229
            && policy.neutralFillGreen == 229
            && policy.neutralFillBlue == 231,
        "advanced neutral fill changed"
        );
    return passed;
}
} // namespace

int main()
{
    bool passed = true;
    const auto& standard =
        SpeakingEvaluationReportTemplateService::policy(
            SpeakingEvaluationReportTemplate::Standard
            );
    const auto& advanced =
        SpeakingEvaluationReportTemplateService::policy(
            SpeakingEvaluationReportTemplate::Advanced
            );

    passed &= expectStandardPolicy(standard);
    passed &= expectAdvancedPolicy(advanced);
    passed &= expect(
        &standard
            == &SpeakingEvaluationReportTemplateService::policy(
                static_cast<SpeakingEvaluationReportTemplate>(99)
                ),
        "unknown template values do not use the standard fallback"
        );
    passed &= expect(
        &standard
            != &advanced,
        "standard and advanced policies share storage"
        );
    return passed ? 0 : 1;
}

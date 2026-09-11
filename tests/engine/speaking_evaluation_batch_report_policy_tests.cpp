#include "classmngr/engine/speaking_evaluation_batch_report_policy.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::SpeakingEvaluationBatchReportPlan;
using classmngr::engine::SpeakingEvaluationBatchReportPolicy;
using classmngr::engine::SpeakingEvaluationBatchReportRequest;
using classmngr::engine::SpeakingEvaluationReportRenderer;
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

    std::cerr << "ClassMngrEngineSpeakingEvaluationBatchReportPolicyTests: "
              << message
              << '\n';
    return false;
}

SpeakingEvaluationBatchReportRequest validRequest()
{
    SpeakingEvaluationBatchReportRequest request;
    request.reportCount = 2;
    request.renderer = SpeakingEvaluationReportRenderer::Internal;
    request.savePdf = true;
    request.hasOutputDirectory = true;
    request.reportTemplates = {
        SpeakingEvaluationReportTemplate::Standard,
        SpeakingEvaluationReportTemplate::Standard
    };
    return request;
}

bool expectError(
    const SpeakingEvaluationBatchReportRequest& request,
    ErrorCode code,
    std::string_view message
    )
{
    const auto result = SpeakingEvaluationBatchReportPolicy::plan(request);
    return expect(!result, "invalid request unexpectedly produced a plan")
        && expect(result.error().code == code, message)
        && expect(result.error().message == message, message);
}
} // namespace

int main()
{
    bool passed = true;

    const auto archiveRequest = validRequest();
    const auto archivePlan = SpeakingEvaluationBatchReportPolicy::plan(
        archiveRequest
        );
    passed &= expect(archivePlan.has_value(), "archive request was rejected");
    if (archivePlan)
    {
        passed &= expect(
            archivePlan->createsBatchArchive
                && !archivePlan->savesIndividualPdfFiles,
            "batch archive output policy changed"
            );
    }

    auto individualRequest = archiveRequest;
    individualRequest.keepIndividualPdfFiles = true;
    const auto individualPlan = SpeakingEvaluationBatchReportPolicy::plan(
        individualRequest
        );
    passed &= expect(individualPlan.has_value(), "individual output was rejected");
    if (individualPlan)
    {
        passed &= expect(
            individualPlan->createsBatchArchive
                && individualPlan->savesIndividualPdfFiles,
            "individual PDF output policy changed"
            );
    }

    auto singleRequest = archiveRequest;
    singleRequest.reportCount = 1;
    singleRequest.reportTemplates = {
        SpeakingEvaluationReportTemplate::Advanced
    };
    const auto singlePlan = SpeakingEvaluationBatchReportPolicy::plan(
        singleRequest
        );
    passed &= expect(singlePlan.has_value(), "single report was rejected");
    if (singlePlan)
    {
        passed &= expect(
            !singlePlan->createsBatchArchive
                && singlePlan->savesIndividualPdfFiles,
            "single report output policy changed"
            );
    }

    auto printOnlyRequest = archiveRequest;
    printOnlyRequest.savePdf = false;
    printOnlyRequest.printReports = true;
    printOnlyRequest.hasOutputDirectory = false;
    const auto printOnlyPlan = SpeakingEvaluationBatchReportPolicy::plan(
        printOnlyRequest
        );
    passed &= expect(
        printOnlyPlan.has_value()
            && !printOnlyPlan->createsBatchArchive
            && !printOnlyPlan->savesIndividualPdfFiles,
        "print-only request policy changed"
        );

    auto emptyRequest = validRequest();
    emptyRequest.reportCount = 0;
    passed &= expectError(
        emptyRequest,
        ErrorCode::InvalidArgument,
        "no-reports"
        );

    auto noModeRequest = validRequest();
    noModeRequest.savePdf = false;
    noModeRequest.printReports = false;
    passed &= expectError(
        noModeRequest,
        ErrorCode::InvalidArgument,
        "output-mode-required"
        );

    auto noDestinationRequest = validRequest();
    noDestinationRequest.hasOutputDirectory = false;
    passed &= expectError(
        noDestinationRequest,
        ErrorCode::InvalidArgument,
        "pdf-destination-required"
        );

    auto exactMultiRequest = validRequest();
    exactMultiRequest.hasExactOutputFilePath = true;
    passed &= expectError(
        exactMultiRequest,
        ErrorCode::InvalidArgument,
        "exact-file-requires-single-report"
        );

    auto mixedTemplateRequest = validRequest();
    mixedTemplateRequest.renderer = SpeakingEvaluationReportRenderer::PowerPoint;
    mixedTemplateRequest.reportTemplates = {
        SpeakingEvaluationReportTemplate::Standard,
        SpeakingEvaluationReportTemplate::Advanced
    };
    passed &= expectError(
        mixedTemplateRequest,
        ErrorCode::Constraint,
        "mixed-powerpoint-templates"
        );

    auto incompleteTemplateRequest = validRequest();
    incompleteTemplateRequest.renderer = SpeakingEvaluationReportRenderer::PowerPoint;
    incompleteTemplateRequest.reportTemplates = {
        SpeakingEvaluationReportTemplate::Standard
    };
    passed &= expectError(
        incompleteTemplateRequest,
        ErrorCode::InvalidArgument,
        "template-count-mismatch"
        );

    passed &= expect(
        SpeakingEvaluationBatchReportPolicy::usesSingleTemplate({}),
        "empty template list should be homogeneous"
        );
    passed &= expect(
        SpeakingEvaluationBatchReportPolicy::usesSingleTemplate({
            SpeakingEvaluationReportTemplate::Advanced,
            SpeakingEvaluationReportTemplate::Advanced
        }),
        "matching template list was rejected"
        );
    passed &= expect(
        !SpeakingEvaluationBatchReportPolicy::usesSingleTemplate({
            SpeakingEvaluationReportTemplate::Standard,
            SpeakingEvaluationReportTemplate::Advanced
        }),
        "mixed template list was accepted"
        );

    return passed ? 0 : 1;
}

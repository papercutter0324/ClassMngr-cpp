#include "classmngr/engine/speaking_evaluation_batch_report_policy.h"

#include <optional>

namespace classmngr::engine
{
namespace
{
std::unexpected<Error> invalidArgument(
    const char* message
    )
{
    return std::unexpected(Error{
        ErrorCode::InvalidArgument,
        message,
        std::nullopt
    });
}

std::unexpected<Error> constraintViolation(
    const char* message
    )
{
    return std::unexpected(Error{
        ErrorCode::Constraint,
        message,
        std::nullopt
    });
}
} // namespace

Result<SpeakingEvaluationBatchReportPlan>
SpeakingEvaluationBatchReportPolicy::plan(
    const SpeakingEvaluationBatchReportRequest& request
    )
{
    if (request.reportCount == 0)
    {
        return invalidArgument("no-reports");
    }

    if (!request.savePdf && !request.printReports)
    {
        return invalidArgument("output-mode-required");
    }

    if (request.savePdf
        && !request.hasOutputDirectory
        && !request.hasExactOutputFilePath)
    {
        return invalidArgument("pdf-destination-required");
    }

    if (request.savePdf
        && request.hasExactOutputFilePath
        && request.reportCount != 1)
    {
        return invalidArgument("exact-file-requires-single-report");
    }

    if (request.renderer == SpeakingEvaluationReportRenderer::PowerPoint)
    {
        if (request.reportTemplates.size() != request.reportCount)
        {
            return invalidArgument("template-count-mismatch");
        }

        if (!usesSingleTemplate(request.reportTemplates))
        {
            return constraintViolation("mixed-powerpoint-templates");
        }
    }

    SpeakingEvaluationBatchReportPlan plan;
    plan.createsBatchArchive = request.savePdf && request.reportCount > 1;
    plan.savesIndividualPdfFiles = request.savePdf
        && (!plan.createsBatchArchive || request.keepIndividualPdfFiles);
    return plan;
}

bool SpeakingEvaluationBatchReportPolicy::usesSingleTemplate(
    const std::vector<SpeakingEvaluationReportTemplate>& reportTemplates
    )
{
    if (reportTemplates.empty())
    {
        return true;
    }

    const SpeakingEvaluationReportTemplate first = reportTemplates.front();
    for (const SpeakingEvaluationReportTemplate reportTemplate : reportTemplates)
    {
        if (reportTemplate != first)
        {
            return false;
        }
    }
    return true;
}

} // namespace classmngr::engine

#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/speaking_evaluation_report_template.h"

#include <cstddef>
#include <vector>

namespace classmngr::engine
{

enum class SpeakingEvaluationReportRenderer
{
    Internal,
    PowerPoint
};

struct SpeakingEvaluationBatchReportRequest
{
    std::size_t reportCount = 0;
    SpeakingEvaluationReportRenderer renderer =
        SpeakingEvaluationReportRenderer::Internal;
    bool savePdf = false;
    bool printReports = false;
    bool keepIndividualPdfFiles = false;
    bool hasOutputDirectory = false;
    bool hasExactOutputFilePath = false;
    std::vector<SpeakingEvaluationReportTemplate> reportTemplates;
};

struct SpeakingEvaluationBatchReportPlan
{
    bool createsBatchArchive = false;
    bool savesIndividualPdfFiles = false;
};

class SpeakingEvaluationBatchReportPolicy final
{
public:
    // This validates only renderer-neutral request constraints. Filesystem
    // creation, localization, PDF rendering, and Office automation remain in
    // the presentation adapter.
    [[nodiscard]] static Result<SpeakingEvaluationBatchReportPlan> plan(
        const SpeakingEvaluationBatchReportRequest& request
        );

    [[nodiscard]] static bool usesSingleTemplate(
        const std::vector<SpeakingEvaluationReportTemplate>& reportTemplates
        );
};

} // namespace classmngr::engine

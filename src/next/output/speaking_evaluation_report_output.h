#pragma once
#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"
#include "next/application/report_job_state.h"
#include "next/resources/resource_loader.h"
#include <expected>
#include <functional>

namespace ClassMngr::Next::Output
{
[[nodiscard]] std::expected<SpeakingEvalReportAssetEnvironment, QString>
resolveSpeakingEvaluationReportAssets(Resources::ResourceLoader&, SpeakingEvalReportTemplate);
[[nodiscard]] std::expected<QString, QString> resolveSpeakingEvaluationPowerPointRoot(
    Resources::ResourceLoader&, SpeakingEvalReportTemplate);

struct SpeakingEvaluationReportRunActions
{
    std::function<bool()> powerPointAvailable;
    std::function<bool(const QString&)> confirmPowerPointRetry;
    std::function<bool()> confirmPowerPointDataAccess;
    std::function<std::expected<QString, QString>(SpeakingEvalReportTemplate)> resolvePowerPointRoot;
};
struct SpeakingEvaluationReportRunResult
{
    DocumentOutputResult output;
    Application::ReportJobSnapshot job;
    int attempts = 0;
};
// The painter/font cache is GUI-thread confined. This synchronous Qt adapter
// pumps bounded coordinator events between reports rather than painting on a
// worker thread; a modal caller supplies responsive progress/cancellation.
[[nodiscard]] SpeakingEvaluationReportRunResult runSpeakingEvaluationReports(
    SpeakingEvalBatchReportService::Request request,
    const SpeakingEvaluationReportRunActions& actions = {});
}

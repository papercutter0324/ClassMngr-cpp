#include "next/output/speaking_evaluation_report_output.h"
#include "next/application/report_job_coordinator.h"
#include <QDir>
#include <QFileInfo>

namespace ClassMngr::Next::Output
{
namespace
{
std::expected<QString, QString> resolve(Resources::ResourceLoader& loader, const QString& id)
{
    const auto resourceId = Resources::ResourceId::fromString(id);
    if (!resourceId) return std::unexpected(QStringLiteral("Invalid report resource ID."));
    const auto request = Resources::ResourceRequest::create(*resourceId,
        Resources::ResourceScopeKind::Operation, QStringLiteral("speaking-evaluation-report"));
    if (!request) return std::unexpected(request.error().message);
    const auto file = loader.resolveFile(*request);
    if (!file) return std::unexpected(file.error().message);
    return file->get();
}
class InPlaceWorker final : public Application::ReportJobWorkerPort
{
public:
    Domain::Result<void> start(UnitCount, Generation generation,
        Application::ReportJobEventSink& sink) override
    { m_sink = &sink; m_generation = generation; m_canceled = false; return Domain::Result<void>::success(); }
    Domain::Result<void> requestCancellation() override
    { m_canceled = true; return Domain::Result<void>::success(); }
    bool m_canceled = false;
    Application::ReportJobEventSink* m_sink = nullptr;
    Generation m_generation;
};
}
std::expected<SpeakingEvalReportAssetEnvironment, QString>
resolveSpeakingEvaluationReportAssets(Resources::ResourceLoader& loader,
    const SpeakingEvalReportTemplate reportTemplate)
{
    const QString prefix = QStringLiteral("resource:assets/templates/speaking-eval/");
    const QString kind = reportTemplate == SpeakingEvalReportTemplate::Advanced
        ? QStringLiteral("advanced-") : QStringLiteral("standard-");
    QString root;
    for (const auto& record : loader.catalog().records())
    {
        if (!record.id().value().startsWith(prefix + kind)) continue;
        auto path = resolve(loader, record.id().value());
        if (!path) return std::unexpected(path.error());
        if (record.id().value().endsWith(QStringLiteral("manifest.json")))
            root = QFileInfo(*path).absolutePath();
    }
    if (root.isEmpty()) return std::unexpected(QStringLiteral("The report template is unavailable."));
    auto handwritten = resolve(loader, QStringLiteral("resource:assets/fonts/JustAnotherHand-Regular.ttf"));
    if (!handwritten) return std::unexpected(handwritten.error());
    SpeakingEvalReportAssetEnvironment environment;
    environment.templateRoot = root;
    environment.handwrittenFontPath = *handwritten;
    return environment;
}
std::expected<QString, QString> resolveSpeakingEvaluationPowerPointRoot(
    Resources::ResourceLoader& loader, const SpeakingEvalReportTemplate reportTemplate)
{
    const auto& layout = speakingEvalReportTemplateLayout(reportTemplate);
    auto path = resolve(loader, QStringLiteral("document.body:") + layout.powerPointResourcePath);
    if (!path) return std::unexpected(path.error());
    QDir directory = QFileInfo(*path).dir();
    directory.cdUp();
    return directory.absolutePath();
}

SpeakingEvaluationReportRunResult runSpeakingEvaluationReports(
    SpeakingEvalBatchReportService::Request request,
    const SpeakingEvaluationReportRunActions& actions)
{
    namespace Service = SpeakingEvalBatchReportService;
    SpeakingEvaluationReportRunResult result;
    InPlaceWorker worker;
    Application::ReportJobCoordinator coordinator(worker);
    const auto progress = request.progressCallback;
    const auto available = [&] { return request.powerPointRenderer ||
        (actions.powerPointAvailable ? actions.powerPointAvailable() : Service::isPowerPointRendererAvailable()); };
    const auto attempt = [&]() {
        ++result.attempts;
        const auto generation = coordinator.startJob(static_cast<std::size_t>(request.reports.size()));
        if (!generation) return DocumentOutputResult{DocumentOutputStatus::Failed,
            QString::fromStdString(generation.error().message)};
        const auto jobGeneration = generation.value();
        request.progressCallback = [&](int completed, int total, const QString& name) {
            static_cast<void>(worker.m_sink->post(Application::ReportJobEvent::progress(jobGeneration,
                static_cast<std::size_t>(completed))));
            static_cast<void>(coordinator.pump());
            if (progress && !progress(completed, total, name))
                static_cast<void>(coordinator.requestCancellation());
            return !worker.m_canceled;
        };
        DocumentOutputResult output;
        if (request.renderer == Service::Renderer::PowerPoint)
        {
            if (!available()) output = {DocumentOutputStatus::Failed, Service::powerPointRendererAvailabilityMessage()};
            else if (actions.confirmPowerPointDataAccess && !actions.confirmPowerPointDataAccess())
                output = {DocumentOutputStatus::Canceled, QObject::tr("Report export was canceled.")};
            else if (request.documentsRoot.isEmpty() && !request.powerPointRenderer && actions.resolvePowerPointRoot)
            {
                auto root = actions.resolvePowerPointRoot(request.reports.isEmpty()
                    ? SpeakingEvalReportTemplate::Standard : request.reports.constFirst().report.reportTemplate);
                if (root) request.documentsRoot = *root;
                else output = {DocumentOutputStatus::Failed, root.error()};
            }
        }
        if (output.message.isEmpty()) output = Service::exportResolvedReports(request);
        if (output.status == DocumentOutputStatus::Completed)
        {
            static_cast<void>(worker.m_sink->post(Application::ReportJobEvent::progress(jobGeneration, request.reports.size())));
            const QString reference = !output.savedArchivePath.isEmpty() ? output.savedArchivePath
                : !output.savedPdfPaths.isEmpty() ? output.savedPdfPaths.first() : QStringLiteral("printed");
            static_cast<void>(worker.m_sink->post(Application::ReportJobEvent::completed(jobGeneration, reference.toStdString())));
        }
        else if (output.status == DocumentOutputStatus::Canceled)
        {
            if (!worker.m_canceled) static_cast<void>(coordinator.requestCancellation());
            static_cast<void>(worker.m_sink->post(Application::ReportJobEvent::cancellationAcknowledged(jobGeneration)));
        }
        else static_cast<void>(worker.m_sink->post(Application::ReportJobEvent::failed(jobGeneration,
            {Domain::ErrorCode::Technical, output.message.toStdString(), true})));
        static_cast<void>(coordinator.pump());
        return output;
    };
    result.output = attempt();
    if (result.output.status == DocumentOutputStatus::InternalRendererFailed && available()
        && actions.confirmPowerPointRetry && actions.confirmPowerPointRetry(result.output.message))
    {
        request.renderer = Service::Renderer::PowerPoint;
        result.output = attempt();
    }
    result.job = coordinator.snapshot();
    return result;
}
}

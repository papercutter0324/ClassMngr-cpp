#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "core/zip_archive_write_adapter.h"
#include "core/qt_pdf_generation_adapter.h"
#include "core/resource_paths.h"
#include "shared/speaking_evaluation/qt_powerpoint_automation_adapter.h"

namespace SpeakingEvalBatchReportService
{
bool isPowerPointRendererAvailable()
{
    return makeQtSpeakingEvalPowerPointAutomationPort()->availability().available;
}

QString powerPointRendererAvailabilityMessage()
{
    return makeQtSpeakingEvalPowerPointAutomationPort()->availability().message;
}

Result exportReports(const Request& input)
{
    Request request = input;
    if (!request.powerPointAutomation)
    {
        request.powerPointAutomation =
            makeQtSpeakingEvalPowerPointAutomationPort();
    }
    if (request.renderer != Renderer::PowerPoint
        || !request.documentsRoot.isEmpty())
    {
        return exportResolvedReports(request);
    }
    auto lease = ResourcePaths::Documents::acquire();
    if (!lease)
    {
        return {Status::Failed, lease.error()};
    }
    Request resolved = request;
    resolved.documentsRoot = ResourcePaths::Documents::directory(*lease);
    return exportResolvedReports(resolved);
}

Result exportResolvedReports(const Request& request)
{
    const auto archiveWriter = makeZipArchiveWritePort();
    return exportResolvedReports(request, archiveWriter.get());
}
Result exportResolvedReports(const Request& request, ArchiveWritePort* archiveWriter)
{
    const auto pdf = makeQtPdfGenerationPort();
    return exportResolvedReports(request, archiveWriter, pdf.get());
}
}

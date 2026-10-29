#include "speaking_eval_batch_report_service.h"
#include "core/resource_paths.h"

namespace SpeakingEvalBatchReportService
{
Result exportReports(const Request& request)
{
    if (request.renderer != Renderer::PowerPoint || request.powerPointRenderer
        || !request.documentsRoot.isEmpty())
        return exportResolvedReports(request);
    auto lease = ResourcePaths::Documents::acquire();
    if (!lease) return {Status::Failed, lease.error()};
    Request resolved = request;
    resolved.documentsRoot = ResourcePaths::Documents::directory(*lease);
    return exportResolvedReports(resolved);
}
}

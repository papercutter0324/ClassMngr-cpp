#include "features/speaking_eval/services/speaking_eval_internal_pdf_renderer.h"
#include "core/qt_pdf_generation_adapter.h"

bool SpeakingEvalInternalPdfRenderer::render(
    const SpeakingEvalReportData& data, const QString& path, QString* error)
{
    const auto pdf = makeQtPdfGenerationPort();
    return render(*pdf, data, path, error);
}

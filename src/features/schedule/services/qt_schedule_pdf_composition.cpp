#include "features/schedule/services/schedule_pdf_renderer.h"
#include "core/qt_pdf_generation_adapter.h"

SchedulePdfRenderer::Result SchedulePdfRenderer::renderSchedulePdf(
    const Request& request, const QString& path)
{
    const auto pdf = makeQtPdfGenerationPort();
    return renderSchedulePdf(*pdf, request, path);
}

#include "shared/roster/roster_template_renderer.h"
#include "core/qt_pdf_generation_adapter.h"

namespace RosterTemplateRenderer {
Result savePagesPdf(const QString& path, TemplateId id, QPageLayout::Orientation orientation,
    const PageProducer& produce, const Fonts& fonts, const PageReleased& released)
{
    const auto pdf = makeQtPdfGenerationPort();
    return savePagesPdf(*pdf, path, id, orientation, produce, fonts, released);
}
Result saveCellValuesPdf(const QString& path, TemplateId id, QPageLayout::Orientation orientation,
    const QList<RosterCellValue>& values, const Fonts& fonts)
{
    const auto pdf = makeQtPdfGenerationPort();
    return saveCellValuesPdf(*pdf, path, id, orientation, values, fonts);
}
Result saveCellValuesPdfForNextSubPrep(const QString& path, TemplateId id,
    QPageLayout::Orientation orientation, const QList<RosterCellValue>& values, const Fonts& fonts)
{
    const auto pdf = makeQtPdfGenerationPort();
    return saveCellValuesPdfForNextSubPrep(*pdf, path, id, orientation, values, fonts);
}
}

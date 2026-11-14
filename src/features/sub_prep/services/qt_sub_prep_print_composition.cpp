#include "features/sub_prep/services/sub_prep_pdf_renderer.h"
#include "features/sub_prep/services/sub_prep_print_service.h"

#include "core/fontmanager.h"
#include "core/qt_pdf_generation_adapter.h"

namespace
{
QFont legacySubPrepInformationSheetFont()
{
    return FontManager::getUiFont(10);
}
}

SubPrepPdfRenderer::Result SubPrepPdfRenderer::renderPdf(
    const SubPrepDocumentModel::Document& document,
    const QString& path
    )
{
    const auto pdf = makeQtPdfGenerationPort();
    return renderPdf(*pdf, document, path, legacySubPrepInformationSheetFont());
}

SubPrepPrintService::Result SubPrepPrintService::saveSubPrepPdf(
    const Request& request,
    const QString& path
    )
{
    const auto pdf = makeQtPdfGenerationPort();
    return saveSubPrepPdf(
        *pdf, request, path, legacySubPrepInformationSheetFont());
}

SubPrepPrintService::Result SubPrepPrintService::printSubPrep(
    const Request& request
    )
{
    const auto pdf = makeQtPdfGenerationPort();
    return printSubPrep(*pdf, request, legacySubPrepInformationSheetFont());
}

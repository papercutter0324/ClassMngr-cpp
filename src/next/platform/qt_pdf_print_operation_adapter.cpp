#include "next/platform/pdf_print_operation.h"

#include "ui/shared/printing/pdf_print_service.h"

#include <memory>

namespace ClassMngr::Next::Platform
{
namespace
{

DialogGeometryPreferencesCallbacks toSharedGeometryPreferences(
    const PrintDialogGeometryPreferences& preferences
    )
{
    return {
        preferences.read,
        preferences.write
    };
}

PdfPrintResult fromSharedResult(const PdfPrintService::Result& result)
{
    PdfPrintStatus status = PdfPrintStatus::Failed;
    switch (result.status)
    {
    case PdfPrintService::Status::Sent:
        status = PdfPrintStatus::Sent;
        break;
    case PdfPrintService::Status::Canceled:
        status = PdfPrintStatus::Canceled;
        break;
    case PdfPrintService::Status::Failed:
    case PdfPrintService::Status::InternalRendererFailed:
        status = PdfPrintStatus::Failed;
        break;
    }
    return {status, result.message};
}

class QtPdfPrintOperation final : public PdfPrintOperation
{
public:
    [[nodiscard]] PdfPrintResult printReadyDocument(
        const ReadyPdfPrintRequest& request
        ) override
    {
        return fromSharedResult(PdfPrintService::printPdfDocument({
            .parent = request.parent,
            .document = request.document,
            .documentPath = request.documentPath,
            .currentPageIndex = request.currentPageIndex,
            .dialogTitle = request.dialogTitle,
            .pageOrientation = request.pageOrientation,
            .fitToPageByDefault = request.fitToPageByDefault,
            .preferredPageSize = request.preferredPageSize,
            .lockPreferredPageSize = request.lockPreferredPageSize,
            .uiFont = request.uiFont,
            .dialogGeometryPreferences = toSharedGeometryPreferences(
                request.dialogGeometryPreferences
                )
        }));
    }

    [[nodiscard]] PdfPrintResult printLocalDocuments(
        const LocalPdfBatchPrintRequest& request
        ) override
    {
        return fromSharedResult(PdfPrintService::printPdfDocuments({
            .parent = request.parent,
            .documentPaths = request.documentPaths,
            .dialogTitle = request.dialogTitle,
            .pageOrientation = request.pageOrientation,
            .preferredPageSize = request.preferredPageSize,
            .fitToPage = request.fitToPage
        }));
    }
};

} // namespace

std::unique_ptr<PdfPrintOperation> makeQtPdfPrintOperation()
{
    return std::make_unique<QtPdfPrintOperation>();
}

} // namespace ClassMngr::Next::Platform

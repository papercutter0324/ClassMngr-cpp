#include "next/platform/pdf_print_operation.h"
#include "core/process_memory_diagnostics.h"

#include "ui/shared/printing/pdf_print_service.h"

#include <memory>
#include <QPdfDocument>

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
    QtPdfPrintOperation() { record(QStringLiteral("print-operation-create")); }
    ~QtPdfPrintOperation() override { record(QStringLiteral("print-operation-release")); }
    [[nodiscard]] PdfPrintResult printReadyDocument(
        const ReadyPdfPrintRequest& request
        ) override
    {
        record(QStringLiteral("print-ready-before"), request.currentPageIndex,
               request.document ? request.document->pageCount() : 0);
        const auto result = fromSharedResult(PdfPrintService::printPdfDocument({
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
        record(QStringLiteral("print-ready-after"), request.currentPageIndex,
               request.document ? request.document->pageCount() : 0,
               static_cast<int>(result.status));
        return result;
    }

    [[nodiscard]] PdfPrintResult printLocalDocuments(
        const LocalPdfBatchPrintRequest& request
        ) override
    {
        record(QStringLiteral("print-batch-before"), -1, request.documentPaths.size());
        const auto result = fromSharedResult(PdfPrintService::printPdfDocuments({
            .parent = request.parent,
            .documentPaths = request.documentPaths,
            .dialogTitle = request.dialogTitle,
            .pageOrientation = request.pageOrientation,
            .preferredPageSize = request.preferredPageSize,
            .fitToPage = request.fitToPage
        }));
        record(QStringLiteral("print-batch-after"), -1, request.documentPaths.size(),
               static_cast<int>(result.status));
        return result;
    }
private:
    void record(const QString& event, int page = -1, int count = 0, int result = -1) noexcept
    {
        try
        {
            recordProcessMemoryDiagnostic(event, QStringLiteral("pdf_print_operation"), m_id, {
                {QStringLiteral("page_index"), page},
                {QStringLiteral("page_or_document_count"), count},
                {QStringLiteral("result"), result}
            });
        }
        catch (...) { }
    }
    quint64 m_id = nextProcessMemoryDiagnosticId();
};

} // namespace

std::unique_ptr<PdfPrintOperation> makeQtPdfPrintOperation()
{
    return std::make_unique<QtPdfPrintOperation>();
}

} // namespace ClassMngr::Next::Platform

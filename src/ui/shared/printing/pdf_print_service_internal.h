#pragma once

#include "ui/shared/printing/pdf_print_service.h"

#include <QPrintDialog>
#include <QPrinter>

#include <functional>

namespace PdfPrintService::Internal
{

enum class BatchDocumentPass
{
    Validation,
    Orientation,
    Rendering
};

// Deterministic hooks for verifying batch mechanics without opening a native
// printer dialog. Production callers use printPdfDocuments(), which supplies
// no hooks and executes the existing dialog and printer paths.
struct BatchPrintTestHooks final
{
    std::function<void(const QString&, BatchDocumentPass)> documentLoadStarted;
    std::function<void(
        const QString&,
        BatchDocumentPass,
        int pageCount
        )> documentReady;
    std::function<void(
        const QString&,
        BatchDocumentPass,
        bool wasReady
        )> documentReleased;
    std::function<int(QPrintDialog&, QPrinter&)> executeDialog;
    std::function<void(
        const QString&,
        int pageIndex,
        int globalPage
        )> pageWillPrint;
    std::function<void(
        const QString&,
        int pageIndex,
        int globalPage,
        QPageLayout::Orientation orientation
        )> firstPageOrientationSelected;
};

[[nodiscard]] Result printPdfDocumentsWithHooks(
    const BatchRequest& request,
    const BatchPrintTestHooks& hooks
    );

} // namespace PdfPrintService::Internal

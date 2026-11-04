#pragma once

#include "next/ui/page_host.h"
#include "next/platform/pdf_print_operation.h"

namespace ClassMngr::Next::Ui
{

class ApplicationShell;

inline constexpr auto kDocumentViewerPageKey = "document_viewer";

[[nodiscard]] PageHostResult<void> registerDocumentViewerPage(
    PageHost& pageHost,
    Platform::PdfPrintOperationFactory printOperationFactory =
        Platform::makeQtPdfPrintOperation
    );

// Connects the existing typed DocumentId selection boundary to the hidden
// viewer route. Catalog folders clear the active document without changing
// the application page-history contract.
void bindDocumentViewerSelection(ApplicationShell& shell);

}  // namespace ClassMngr::Next::Ui

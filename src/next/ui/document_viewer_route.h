#pragma once

#include "next/ui/page_host.h"

namespace ClassMngr::Next::Ui
{

class ApplicationShell;

inline constexpr auto kDocumentViewerPageKey = "document_viewer";

[[nodiscard]] PageHostResult<void> registerDocumentViewerPage(
    PageHost& pageHost
    );

// Connects the existing typed DocumentId selection boundary to the hidden
// viewer route. Catalog folders clear the active document without changing
// the application page-history contract.
void bindDocumentViewerSelection(ApplicationShell& shell);

}  // namespace ClassMngr::Next::Ui

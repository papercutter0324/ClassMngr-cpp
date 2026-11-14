#pragma once

#include "next/platform/pdf_print_operation.h"

#include <QImage>
#include <QSize>

#include <functional>
#include <memory>

namespace ClassMngr::Next::Platform
{

struct RosterPdfPrintRequest final
{
    QWidget* parent = nullptr;
    QString dialogTitle;
    QPageLayout::Orientation pageOrientation = QPageLayout::Portrait;
    bool fitToPageByDefault = false;
    std::optional<QPageSize::PageSizeId> preferredPageSize;
    bool lockPreferredPageSize = false;
    QFont uiFont;
    PrintDialogGeometryPreferences dialogGeometryPreferences;
};

// Operation-scoped owner for a generated roster PDF. The implementation owns
// its temporary file, QtPdf document, and all document access. Printing borrows
// the loaded document only for the synchronous operation call.
class RosterPdfPreviewSession
{
public:
    virtual ~RosterPdfPreviewSession() = default;

    [[nodiscard]] virtual bool isValid() const noexcept = 0;
    [[nodiscard]] virtual QString path() const = 0;
    [[nodiscard]] virtual bool loadGeneratedPdf() = 0;
    virtual void release() noexcept = 0;
    [[nodiscard]] virtual int pageCount() const noexcept = 0;
    [[nodiscard]] virtual QImage renderPage(
        int pageIndex,
        const QSize& maximumSize
        ) = 0;
    [[nodiscard]] virtual PdfPrintResult printReadyDocument(
        PdfPrintOperation& operation,
        const RosterPdfPrintRequest& request
        ) = 0;
};

using RosterPdfPreviewSessionFactory = std::function<
    std::unique_ptr<RosterPdfPreviewSession>()>;

[[nodiscard]] std::unique_ptr<RosterPdfPreviewSession>
    makeQtRosterPdfPreviewSession();

} // namespace ClassMngr::Next::Platform

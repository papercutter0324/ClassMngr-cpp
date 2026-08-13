#include "ui/shared/printing/pdf_print_dialog.h"

#include "ui/shared/printing/pdf_print_dialog_internal.h"

#include <QEvent>
#include <QFileInfo>
#include <QLineEdit>

#include <utility>

PdfPrintDialog::PdfPrintDialog(
    QWidget* parent,
    QPdfDocument* document,
    const QString& documentPath,
    PdfPrintDialogSupport::RenderFunction renderFunction,
    int currentPageIndex,
    QPageLayout::Orientation pageOrientation,
    bool fitToPageByDefault,
    std::optional<QPageSize::PageSizeId> preferredPageSize,
    bool lockPreferredPageSize
    )
    : DialogShell(QStringLiteral("pdfPrint"), parent)
    , m_document(document)
    , m_documentPath(documentPath)
    , m_currentPageIndex(currentPageIndex)
    , m_pageOrientation(pageOrientation)
    , m_fitToPageByDefault(fitToPageByDefault)
    , m_preferredPageSize(preferredPageSize)
    , m_lockPreferredPageSize(lockPreferredPageSize)
    , m_renderFunction(std::move(renderFunction))
    , m_printResult{
          PdfPrintService::Status::Canceled,
          QString()
      }
    , m_printer(QPrinter::HighResolution)
{
    buildUi();
    populatePrinters();
    connectSignals();
    configurePrinterFromUi();
    updatePrinterCapabilities();
    updatePageRangeControls();
    updateValidation();
    updatePreview();
}

PdfPrintService::Result PdfPrintDialog::printResult() const
{
    return m_printResult;
}

bool PdfPrintDialog::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        watched == m_customPagesEdit
        && event->type() == QEvent::FocusIn
        && m_customPagesEdit->text()
            == PdfPrintDialogPrivate::customPagesSample()
        )
    {
        m_customPagesEdit->clear();
    }

    return QDialog::eventFilter(
        watched,
        event
        );
}

QString PdfPrintDialog::documentDisplayName() const
{
    const QString fileName =
        QFileInfo(m_documentPath).fileName();

    return fileName.trimmed().isEmpty()
        ? tr("PDF Document")
        : fileName;
}

QString PdfPrintDialog::printJobTitle() const
{
    return tr("Print from ClassMngr - %1")
        .arg(
            documentDisplayName()
            );
}

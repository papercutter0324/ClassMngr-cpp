#include "pdf_viewer_page_p.h"

void PdfViewerPage::applyPageInput()
{
    const int pageCount =
        m_document->pageCount();

    if (pageCount <= 0)
    {
        updatePageDisplay();
        return;
    }

    bool ok = false;
    const int requestedPage =
        m_pageInput->text()
            .trimmed()
            .toInt(&ok);

    if (!ok)
    {
        updatePageDisplay();
        return;
    }

    const int targetPage =
        std::clamp(
            requestedPage,
            1,
            pageCount
            );

    QPdfPageNavigator* navigator =
        m_view->pageNavigator();

    navigator->jump(
        targetPage - 1,
        {},
        navigator->currentZoom()
        );

    updatePageDisplay();
}

void PdfViewerPage::handleDocumentStatusChanged()
{
    if (
        m_tearingDown
        || m_documentReleased
        || !m_document
        || !m_view
        )
    {
        return;
    }

    if (m_document->status() == QPdfDocument::Status::Ready)
    {
        m_view->setPageMode(
            QPdfView::PageMode::MultiPage
            );

        clearStatusMessage();
        resetZoom();
        updatePageDisplay();
        updateDocumentActionButtons();
        notifyDocumentLoaded();
        return;
    }

    if (m_document->status() == QPdfDocument::Status::Error)
    {
        showStatusMessage(
            tr("Failed to load PDF: %1")
                .arg(documentErrorText())
            );
    }

    updatePageDisplay();
    updateDocumentActionButtons();
}


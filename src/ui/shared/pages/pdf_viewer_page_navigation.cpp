#include "pdf_viewer_page_p.h"

#include "core/startup_profiler.h"

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
        if (m_documentContentToken)
        {
            [[maybe_unused]] const auto ready =
                m_documentContentSession.markReady(
                    *m_documentContentToken
                    );
        }

        if (!m_pdfLoadRecorded && m_document->pageCount() > 0)
        {
            StartupProfiler::recordPdfDocumentLoaded(
                m_currentFilePath,
                m_document->pageCount()
                );
            m_pdfLoadRecorded = true;
        }

        m_view->setPageMode(
            QPdfView::PageMode::MultiPage
            );

        clearStatusMessage();
        resetZoom();
        updatePageDisplay();
        updateDocumentActionButtons();
        return;
    }

    if (m_document->status() == QPdfDocument::Status::Error)
    {
        const QString errorText = documentErrorText();
        failDocumentContentSession(errorText);
        showStatusMessage(
            tr("Failed to load PDF: %1")
                .arg(errorText)
            );
    }

    updatePageDisplay();
    updateDocumentActionButtons();
}


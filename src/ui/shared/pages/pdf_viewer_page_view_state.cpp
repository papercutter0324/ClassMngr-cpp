#include "pdf_viewer_page_p.h"

void PdfViewerPage::applyUiFonts()
{
    const QFont standardFont =
        font();

    QFont smallFont =
        standardFont;

    smallFont.setPointSize(
        FontManager::adjustedPointSize(10)
        );

    for (QLabel* label : {
             m_statusLabel,
             m_pageLabel,
             m_pageTotalLabel,
             m_zoomLabel
         })
    {
        if (label)
        {
            label->setFont(
                smallFont
                );
        }
    }

    const auto applyStandardFont =
        [&standardFont](QWidget* widget)
    {
        if (widget)
        {
            widget->setFont(
                standardFont
                );
        }
    };

    applyStandardFont(m_pageInput);
    applyStandardFont(m_zoomInput);
    applyStandardFont(m_zoomOutButton);
    applyStandardFont(m_zoomInButton);
    applyStandardFont(m_fitWidthButton);
    applyStandardFont(m_fitPageButton);
    applyStandardFont(m_exportButton);
    applyStandardFont(m_printButton);

    if (QHBoxLayout* layout = bottomLayout())
    {
        layout->invalidate();
    }

    updateGeometry();
}

void PdfViewerPage::applyZoom()
{
    m_currentZoom =
        normalizedZoomFactor(
            m_currentZoom
            );

    m_view->setZoomFactor(
        m_currentZoom
        );

    updateZoomDisplay();
}

void PdfViewerPage::updateZoomDisplay()
{
    m_zoomInput->setText(
        zoomText(m_currentZoom)
        );
}

void PdfViewerPage::applyDocumentViewerBackground()
{
    if (!m_view)
    {
        return;
    }

    const QString background =
        documentViewerBackgroundProperty(
            m_documentViewerBackground
            );

    m_view->setProperty(
        "pdfViewerBackground",
        background
        );
    refreshStyle(
        m_view
        );

    if (m_documentViewerBackground == DocumentViewerBackground::White)
    {
        setPdfViewBackgroundColor(
            m_view,
            QColor(Qt::white)
            );
    }
    else if (m_documentViewerBackground == DocumentViewerBackground::Black)
    {
        setPdfViewBackgroundColor(
            m_view,
            QColor(Qt::black)
            );
    }
    else
    {
        resetPdfViewBackgroundColor(
            m_view
            );
    }

    if (m_view->viewport())
    {
        m_view->viewport()->setProperty(
            "pdfViewerBackground",
            background
            );
        refreshStyle(
            m_view->viewport()
            );
        m_view->viewport()->update();
    }

    m_view->update();
}

void PdfViewerPage::applyCalculatedFitZoom()
{
    if (
        m_tearingDown
        || !m_view
        || !m_zoomInput
        )
    {
        return;
    }

    m_currentZoom =
        normalizedZoomFactor(
            effectiveZoomFactor()
            );

    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );
    m_view->setZoomFactor(
        m_currentZoom
        );
    updateZoomDisplay();
}

qreal PdfViewerPage::effectiveZoomFactor() const
{
    if (!m_view)
    {
        return m_currentZoom;
    }

    if (m_view->zoomMode() == QPdfView::ZoomMode::Custom)
    {
        return m_view->zoomFactor();
    }

    if (
        !m_document
        || m_document->status() != QPdfDocument::Status::Ready
        || m_document->pageCount() <= 0
        || !m_view->viewport()
        )
    {
        return m_view->zoomFactor();
    }

    int pageIndex =
        0;

    if (m_view->pageNavigator())
    {
        pageIndex =
            std::clamp(
                m_view->pageNavigator()->currentPage(),
                0,
                m_document->pageCount() - 1
                );
    }

    const QSizeF pagePointSize =
        m_document->pagePointSize(pageIndex);

    if (
        !pagePointSize.isValid()
        || pagePointSize.isEmpty()
        )
    {
        return m_view->zoomFactor();
    }

    const QMargins margins =
        m_view->documentMargins();
    const QSize viewportSize =
        m_view->viewport()->size();

    const qreal availableWidth =
        viewportSize.width()
        - margins.left()
        - margins.right();
    const qreal availableHeight =
        viewportSize.height()
        - margins.top()
        - margins.bottom();

    if (
        availableWidth <= 0.0
        || availableHeight <= 0.0
        )
    {
        return m_view->zoomFactor();
    }

    const qreal horizontalPixelsPerPoint =
        std::max(
            m_view->logicalDpiX() / 72.0,
            0.01
            );
    const qreal verticalPixelsPerPoint =
        std::max(
            m_view->logicalDpiY() / 72.0,
            0.01
            );

    const qreal widthZoom =
        availableWidth
        / (pagePointSize.width() * horizontalPixelsPerPoint);
    const qreal heightZoom =
        availableHeight
        / (pagePointSize.height() * verticalPixelsPerPoint);

    if (m_view->zoomMode() == QPdfView::ZoomMode::FitToWidth)
    {
        return std::max(
            widthZoom,
            0.01
            );
    }

    return std::max(
        std::min(
            widthZoom,
            heightZoom
            ),
        0.01
        );
}

void PdfViewerPage::updatePageDisplay()
{
    if (
        m_tearingDown
        || !m_document
        || !m_view
        || !m_view->pageNavigator()
        || !m_pageValidator
        || !m_pageInput
        || !m_pageTotalLabel
        )
    {
        return;
    }

    const int pageCount =
        m_document->pageCount();
    const bool hasPages =
        pageCount > 0;

    m_pageValidator->setRange(
        1,
        std::max(
            1,
            pageCount
            )
        );

    const QSignalBlocker pageInputBlocker(
        m_pageInput
        );

    if (!hasPages)
    {
        m_pageInput->setText(
            QStringLiteral("0")
            );
        m_pageInput->setEnabled(false);
        m_pageTotalLabel->setText(
            tr("of 0")
            );
        return;
    }

    const int currentPage =
        std::clamp(
            m_view->pageNavigator()->currentPage(),
            0,
            pageCount - 1
            ) + 1;

    m_pageInput->setText(
        QString::number(currentPage)
        );
    m_pageInput->setEnabled(true);
    m_pageTotalLabel->setText(
        tr("of %1").arg(pageCount)
        );
}

void PdfViewerPage::updateDocumentActionButtons()
{
    const bool hasFile =
        !m_currentFilePath.trimmed().isEmpty();
    const bool canExport =
        hasFile
        && m_documentDescriptor.exportEnabled
        && !m_documentDescriptor.exportFilePath.trimmed().isEmpty();
    const bool canPrint =
        hasFile
        && m_document
        && m_document->status() == QPdfDocument::Status::Ready
        && m_document->pageCount() > 0
        && m_documentDescriptor.printEnabled;

    m_exportButton->setVisible(
        true
        );
    m_exportButton->setEnabled(
        canExport
        );
    m_exportButton->setToolTip(
        canExport
            ? tr("Export this file")
            : tr("Export is not available for this file")
        );

    m_printButton->setVisible(
        true
        );
    m_printButton->setEnabled(
        canPrint
        );
    m_printButton->setToolTip(
        canPrint
            ? tr("Print this file")
            : tr("Print is not available for this file")
        );
}


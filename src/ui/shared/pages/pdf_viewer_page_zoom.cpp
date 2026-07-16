#include "pdf_viewer_page_p.h"

void PdfViewerPage::zoomIn()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );

    m_currentZoom *=
        ZoomStep;

    applyZoom();
}

void PdfViewerPage::zoomOut()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );

    m_currentZoom /=
        ZoomStep;

    applyZoom();
}

void PdfViewerPage::resetZoom()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );

    m_currentZoom =
        1.0;

    applyZoom();
}

void PdfViewerPage::fitWidth()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::FitToWidth
        );

    applyCalculatedFitZoom();
}

void PdfViewerPage::fitPage()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::FitInView
        );

    applyCalculatedFitZoom();
}

void PdfViewerPage::applyZoomInput()
{
    QString text =
        m_zoomInput->text();

    text.remove(
        QLatin1Char('%')
        );

    bool ok = false;
    const qreal zoom =
        text.trimmed().toDouble(&ok) / 100.0;

    if (!ok)
    {
        updateZoomDisplay();
        return;
    }

    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );

    m_currentZoom =
        zoom;

    applyZoom();
}


#include "pdf_viewer_page_p.h"

PdfViewerPage::PdfViewerPage(
    QWidget* parent
    )
    : BasePage(parent)
{
    buildUi();
}

PdfViewerPage::~PdfViewerPage()
{
    m_tearingDown =
        true;

    if (
        m_view
        && m_view->pageNavigator()
        )
    {
        disconnect(
            m_view->pageNavigator(),
            nullptr,
            this,
            nullptr
            );
    }

    if (m_document)
    {
        disconnect(
            m_document,
            nullptr,
            this,
            nullptr
            );
    }

    delete m_view;
    m_view =
        nullptr;

    if (m_document)
    {
        m_document->close();
        delete m_document;
        m_document =
            nullptr;
    }
}

void PdfViewerPage::retranslateUi()
{
    if (m_pageLabel)
    {
        m_pageLabel->setText(
            tr("Page:")
            );
    }

    if (m_pageInput)
    {
        m_pageInput->setToolTip(
            tr("Go to page")
            );
    }

    if (m_zoomLabel)
    {
        m_zoomLabel->setText(
            tr("Zoom:")
            );
    }

    if (m_zoomOutButton)
    {
        m_zoomOutButton->setToolTip(
            tr("Zoom out")
            );
    }

    if (m_zoomInButton)
    {
        m_zoomInButton->setToolTip(
            tr("Zoom in")
            );
    }

    if (m_fitWidthButton)
    {
        m_fitWidthButton->setText(
            tr("Fit Width")
            );
    }

    if (m_fitPageButton)
    {
        m_fitPageButton->setText(
            tr("Fit Page")
            );
    }

    if (m_exportButton)
    {
        m_exportButton->setText(
            tr("Export")
            );
    }

    if (m_printButton)
    {
        m_printButton->setText(
            tr("Print")
            );
    }

    applyUiFonts();
    updatePageDisplay();
    updateDocumentActionButtons();
}

void PdfViewerPage::changeEvent(
    QEvent* event
    )
{
    BasePage::changeEvent(event);

    if (
        !m_tearingDown
        && event->type() == QEvent::FontChange
        )
    {
        applyUiFonts();
    }
}

bool PdfViewerPage::loadPdf(
    const QString& filePath,
    PdfViewerDocumentActions actions
    )
{
    m_currentFilePath =
        filePath;
    m_documentActions =
        actions;

    if (filePath.trimmed().isEmpty())
    {
        m_document->close();
        updateDocumentActionButtons();
        updatePageDisplay();
        showStatusMessage(
            tr("No PDF file selected.")
            );
        return false;
    }

    m_document->close();
    updateDocumentActionButtons();

    const QPdfDocument::Error error =
        m_document->load(filePath);

    if (error != QPdfDocument::Error::None)
    {
        updateDocumentActionButtons();
        updatePageDisplay();
        showStatusMessage(
            tr("Failed to load PDF: %1")
                .arg(
                    documentErrorText(
                        static_cast<int>(error)
                        )
                    )
            );
        return false;
    }

    if (m_document->status() == QPdfDocument::Status::Ready)
    {
        handleDocumentStatusChanged();
    }

    return true;
}

QString PdfViewerPage::currentFilePath() const
{
    return m_currentFilePath;
}

void PdfViewerPage::setDocumentPageSpacing(
    DocumentPageSpacing spacing
    )
{
    m_documentPageSpacing =
        spacing;

    if (!m_view)
    {
        return;
    }

    m_view->setPageSpacing(
        documentPageSpacingPixels(
            m_documentPageSpacing
            )
        );
}

void PdfViewerPage::setDocumentViewerBackground(
    DocumentViewerBackground background
    )
{
    m_documentViewerBackground =
        background;

    applyDocumentViewerBackground();
}

#include "pdf_viewer_page_p.h"

#include "core/startup_profiler.h"

#include <utility>

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
        if (m_pdfLoadRecorded)
        {
            StartupProfiler::recordPdfDocumentReleased(
                m_currentFilePath
                );
            m_pdfLoadRecorded = false;
        }
        m_document->close();
        releaseDocumentContentSession();
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

    applyUiFonts();
    updatePageDisplay();
    updateDocumentActionButtons();
}

PageOutputCapabilities PdfViewerPage::outputCapabilities() const
{
    const bool hasFile =
        !m_currentFilePath.trimmed().isEmpty();
    const bool canSave =
        hasFile
        && m_documentDescriptor.exportEnabled
        && !m_documentDescriptor.exportFilePath.trimmed().isEmpty();
    const bool canPrint =
        hasFile
        && m_document
        && m_document->status() == QPdfDocument::Status::Ready
        && m_document->pageCount() > 0
        && m_documentDescriptor.printEnabled;

    return {canPrint, canSave};
}

void PdfViewerPage::printCurrentPage()
{
    printFile();
}

void PdfViewerPage::saveCurrentPageAs()
{
    exportFile();
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
    PdfViewerDocumentDescriptor descriptor
    )
{
    const QString filePath =
        descriptor.pdfFilePath;

    releaseDocument();

    if (descriptor.contentReference)
    {
        auto request = m_documentContentSession.request(
            *descriptor.contentReference
            );
        if (!request)
        {
            showStatusMessage(
                QString::fromUtf8(request.error().message)
                );
            return false;
        }

        m_documentContentToken = request.value();
        const auto loading = m_documentContentSession.beginLoading(
            *m_documentContentToken
            );
        if (!loading)
        {
            releaseDocumentContentSession();
            showStatusMessage(
                QString::fromUtf8(loading.error().message)
                );
            return false;
        }
    }

    if (filePath.trimmed().isEmpty())
    {
        const QString errorText = tr("No PDF file selected.");
        failDocumentContentSession(errorText);
        showStatusMessage(errorText);
        return false;
    }

    m_documentReleased = false;
    m_pdfLoadRecorded = false;
    m_currentFilePath = filePath;
    m_documentDescriptor = std::move(descriptor);
    m_view->setDocument(m_document);

    const QPdfDocument::Error error =
        m_document->load(filePath);

    if (error != QPdfDocument::Error::None)
    {
        const QString errorText = documentErrorText(
            static_cast<int>(error)
            );
        failDocumentContentSession(errorText);
        updateDocumentActionButtons();
        updatePageDisplay();
        showStatusMessage(
            tr("Failed to load PDF: %1")
                .arg(errorText)
            );
        return false;
    }

    if (m_document->status() == QPdfDocument::Status::Ready)
    {
        handleDocumentStatusChanged();
    }

    return true;
}

void PdfViewerPage::releaseDocument()
{
    if (!m_document)
    {
        return;
    }

    const bool documentWasLoaded =
        m_pdfLoadRecorded;
    const QString releasedFilePath =
        m_currentFilePath;

    m_documentReleased = true;
    m_pdfLoadRecorded = false;

    // Keep the view attached to the document while closing it. Qt 6.12's
    // QPdfView tears down its internal bookmark model from setDocument(nullptr)
    // and can dereference that model during the next event-loop turn. Closing
    // the document releases the loaded PDF pages while retaining the stable
    // view/document pairing needed for a later reopen.
    m_document->close();
    releaseDocumentContentSession();

    if (documentWasLoaded)
    {
        StartupProfiler::recordPdfDocumentReleased(
            releasedFilePath
            );
    }
    m_currentFilePath.clear();
    m_documentDescriptor = {};
    m_currentZoom = 1.0;

    if (m_view)
    {
        m_view->setZoomMode(QPdfView::ZoomMode::Custom);
        m_view->setZoomFactor(m_currentZoom);
    }

    updateZoomDisplay();
    updatePageDisplay();
    clearStatusMessage();
    updateDocumentActionButtons();
}

QString PdfViewerPage::currentFilePath() const
{
    return m_currentFilePath;
}

bool PdfViewerPage::hasLoadedDocument() const
{
    return !m_documentReleased
        && m_document
        && m_document->status() == QPdfDocument::Status::Ready
        && m_document->pageCount() > 0;
}

ClassMngr::Next::Application::DocumentContentSnapshot
PdfViewerPage::documentContentSnapshot() const
{
    return m_documentContentSession.snapshot();
}

void PdfViewerPage::failDocumentContentSession(
    const QString& errorText
    )
{
    if (!m_documentContentToken)
    {
        return;
    }

    [[maybe_unused]] const auto failed = m_documentContentSession.fail(
        *m_documentContentToken,
        {
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = errorText.toUtf8().toStdString(),
            .recoverable = true
        }
        );
}

void PdfViewerPage::releaseDocumentContentSession()
{
    if (!m_documentContentToken)
    {
        return;
    }

    [[maybe_unused]] const auto released =
        m_documentContentSession.release();
    m_documentContentToken.reset();
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

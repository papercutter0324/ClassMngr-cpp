#include "pdf_viewer_page_p.h"

#include "core/memory_usage_diagnostics.h"

#include <utility>

PdfViewerPage::PdfViewerPage(
    QWidget* parent
    )
    : BasePage(parent)
{
    MemoryUsageDiagnostics::registerMemoryBreakdownProvider(this, this);
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

    if (filePath.trimmed().isEmpty())
    {
        showStatusMessage(
            tr("No PDF file selected.")
            );
        return false;
    }

    m_documentReleased = false;
    m_documentLoadRecorded = false;
    m_documentLoadTimed = MemoryUsageDiagnostics::isEnabled();
    if (m_documentLoadTimed)
    {
        m_documentLoadTimer.start();
    }
    m_documentByteCount = static_cast<quint64>(
        qMax<qint64>(0, QFileInfo(filePath).size())
        );
    m_currentFilePath = filePath;
    m_documentDescriptor = std::move(descriptor);
    m_view->setDocument(m_document);

    const QPdfDocument::Error error =
        m_document->load(filePath);

    if (error != QPdfDocument::Error::None)
    {
        if (m_documentLoadTimed)
        {
            MemoryUsageDiagnostics::recordTimedOperation(
                QStringLiteral("pdf-open"),
                QStringLiteral("failed before ready"),
                m_documentLoadTimer.elapsed()
                );
            m_documentLoadTimed = false;
        }
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

void PdfViewerPage::releaseDocument()
{
    if (!m_document)
    {
        return;
    }

    const bool recordTiming = MemoryUsageDiagnostics::isEnabled();
    QElapsedTimer releaseTimer;
    if (recordTiming)
    {
        releaseTimer.start();
    }

    m_documentReleased = true;

    const bool wasLoaded = m_documentLoadRecorded;
    m_documentLoadRecorded = false;
    m_documentLoadTimed = false;

    if (m_view && m_view->document() == m_document)
    {
        m_view->setDocument(nullptr);
    }

    m_document->close();
    m_currentFilePath.clear();
    m_documentDescriptor = {};
    m_documentByteCount = 0;
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

    if (wasLoaded)
    {
        emit documentReleased();
        MemoryUsageDiagnostics::recordEvent(QStringLiteral("pdf-released"));
        if (recordTiming)
        {
            MemoryUsageDiagnostics::recordTimedOperation(
                QStringLiteral("pdf-release"),
                QStringLiteral("loaded document"),
                releaseTimer.elapsed()
                );
        }
    }
}

void PdfViewerPage::notifyDocumentLoaded()
{
    if (m_documentLoadRecorded || m_currentFilePath.trimmed().isEmpty())
    {
        return;
    }

    m_documentLoadRecorded = true;
    const quint64 byteCount = static_cast<quint64>(
        qMax<qint64>(0, QFileInfo(m_currentFilePath).size())
        );
    emit documentLoaded(byteCount);
    MemoryUsageDiagnostics::recordEvent(
        QStringLiteral("pdf-loaded"),
        QStringLiteral("bytes=%1").arg(byteCount)
        );
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

QList<MemoryBreakdownEntry> PdfViewerPage::memoryBreakdown() const
{
    const bool loaded = hasLoadedDocument();
    const int pageCount = loaded ? m_document->pageCount() : 0;

    return {
        {
            QStringLiteral("Loaded PDF source"),
            QStringLiteral("PDF Viewer"),
            loaded ? m_documentByteCount : quint64{0},
            loaded ? quint64{1} : quint64{0},
            loaded
                ? QStringLiteral("loaded; pages=%1; source bytes=%2")
                      .arg(pageCount)
                      .arg(m_documentByteCount)
                : QStringLiteral("released"),
            true
        }
    };
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

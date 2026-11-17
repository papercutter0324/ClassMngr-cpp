#include "next/platform/qt_pdf_viewer_session.h"

#include <QEventLoop>
#include <QPdfView>
#include <QPdfPageNavigator>
#include <QSizePolicy>
#include <QTimer>
#include <QWidget>

#include <utility>

namespace ClassMngr::Next::Platform
{

namespace
{
bool isTerminalCloseStatus(QPdfDocument::Status status)
{
    return status == QPdfDocument::Status::Null
        || status == QPdfDocument::Status::Error;
}
}

std::unique_ptr<PdfViewerSession> makeQtPdfViewerSession(
    Resources::ResourceLoader& loader, QWidget* viewParent)
{
    return std::make_unique<QtPdfViewerSession>(loader, viewParent);
}

QtPdfViewerSession::QtPdfViewerSession(
    Resources::ResourceLoader& loader, QWidget* viewParent,
    std::shared_ptr<ProcessMemoryDiagnostics> diagnostics
    )
    : QObject(viewParent), m_diagnostics(std::move(diagnostics)),
      m_resourcePort(loader), m_viewParent(viewParent)
{
    record(QStringLiteral("qpdf-session-create"), QStringLiteral("before"));
    createDocument();
    createView();
    record(QStringLiteral("qpdf-session-create"), QStringLiteral("after"));
}

QtPdfViewerSession::~QtPdfViewerSession()
{
    m_statusHandler = {};
    releaseConsumers(false);
    record(QStringLiteral("qpdf-session-release"), QStringLiteral("after"));
}

void QtPdfViewerSession::createDocument()
{
    m_document = std::make_unique<QPdfDocument>(this);
    record(QStringLiteral("qpdf-document-create"), QStringLiteral("after"));
    const quint64 generation = m_generation;
    connect(m_document.get(), &QPdfDocument::statusChanged, this,
            [this, generation](QPdfDocument::Status status) {
        if (!m_releasing && generation == m_generation)
        {
            record(QStringLiteral("qpdf-document-status"), QStringLiteral("observed"));
        }
        // Error cleanup may release the device. Never run it on load()'s
        // stack; both generation and current status must still match.
        QTimer::singleShot(0, this, [this, generation, status] {
            publishStatus(status, generation);
        });
    });
}

void QtPdfViewerSession::createView()
{
    m_view = std::make_unique<QPdfView>(m_viewParent);
    m_view->setObjectName(QStringLiteral("documentViewerPdfView"));
    m_view->setDocument(m_document.get());
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::Custom);
    m_view->setZoomFactor(1.0);
    m_view->setPageSpacing(8);
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (m_view->pageNavigator())
    {
        connect(m_view->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
                this, [this](int) {
            ++m_pageSelectionCount;
            if (m_pageSelectionCount <= 32)
                record(QStringLiteral("qpdf-view-page-selection"), QStringLiteral("observed"));
        });
    }
    record(QStringLiteral("qpdf-view-create"), QStringLiteral("after"));
}

Domain::Result<bool> QtPdfViewerSession::open(
    const Domain::DocumentContentReference& reference,
    const QString& sessionLabel
    )
{
    if (m_borrowCount || m_releasing)
    {
        return Domain::Result<bool>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The PDF viewer is busy with the active document.",
            .recoverable = true
        });
    }
    static_cast<void>(release());
    record(QStringLiteral("qpdf-open"), QStringLiteral("before"));
    auto content = m_resourcePort.openPrimaryPdf(reference, sessionLabel);
    if (!content)
    {
        record(QStringLiteral("qpdf-open"), QStringLiteral("failed"));
        return Domain::Result<bool>::failure(content.error());
    }
    m_content.emplace(std::move(content.value()));
    record(QStringLiteral("primary-stream-open"), QStringLiteral("after"));
    QIODevice& device = m_content->primaryBody.get().device();
    if (!device.isOpen() || !device.isReadable() || !device.seek(0))
    {
        record(QStringLiteral("qpdf-open"), QStringLiteral("failed"));
        static_cast<void>(release());
        return Domain::Result<bool>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unable to read the selected PDF.",
            .recoverable = true
        });
    }
    const quint64 generation = m_generation;
    record(QStringLiteral("qpdf-document-load"), QStringLiteral("before"));
    if (m_documentLoadForTesting)
    {
        m_documentLoadForTesting(*m_document);
    }
    else
    {
        m_document->load(&device);
    }
    record(QStringLiteral("qpdf-document-load"), QStringLiteral("after"));
    // Match the synchronous Ready contract without dispatching Error cleanup
    // until load() has unwound. Duplicate queued Ready events are suppressed.
    if (status() == QPdfDocument::Status::Ready)
    {
        publishStatus(QPdfDocument::Status::Ready, generation);
    }
    record(QStringLiteral("qpdf-open"), QStringLiteral("after"));
    return Domain::Result<bool>::success(true);
}

void QtPdfViewerSession::publishStatus(
    QPdfDocument::Status value, quint64 generation
    )
{
    if (m_releasing || generation != m_generation || value != status()
        || m_publishedStatus == value)
    {
        return;
    }
    m_publishedStatus = value;
    record(QStringLiteral("qpdf-document-status"), QStringLiteral("published"));
    if (m_statusHandler)
    {
        m_statusHandler(value);
    }
}

bool QtPdfViewerSession::release()
{
    if (m_borrowCount || m_releasing)
    {
        return false;
    }
    releaseConsumers(true);
    return true;
}

void QtPdfViewerSession::releaseConsumers(bool recreate)
{
    if (m_releasing)
    {
        return;
    }
    m_releasing = true;
    const quint64 retiredGeneration = m_generation;
    const int retiredPages = pageCount();
    ++m_generation;
    m_publishedStatus.reset();
    bool timedOut = false;
    if (m_document && status() != QPdfDocument::Status::Null)
    {
        record(QStringLiteral("qpdf-document-close"), QStringLiteral("before"));
        m_document->close();
        timedOut = !waitForCloseCompletion();
        record(QStringLiteral("qpdf-document-close"), QStringLiteral("after"));
        if (timedOut)
        {
            record(QStringLiteral("qpdf-document-close"), QStringLiteral("timeout"));
        }
    }
    // A timeout or final teardown destroys the rendering consumer before the
    // document, with the stream still alive.
    if (m_view && (timedOut || !recreate))
    {
        record(QStringLiteral("qpdf-view-destroy"), QStringLiteral("before"));
        m_view.reset();
        record(QStringLiteral("qpdf-view-destroy"), QStringLiteral("after"));
    }
    auto previousDocument = std::move(m_document);
    if (recreate && m_view)
    {
        // Qt 6.12's QPdfView cannot safely accept a null document. Rebind the
        // existing widget to a live, empty document before retiring the old
        // document and its stream; this also preserves its layout and settings.
        createDocument();
        m_view->setDocument(m_document.get());
    }
    if (previousDocument)
    {
        const auto retiredStatus = previousDocument->status();
        recordRetiredDocument(QStringLiteral("before"), retiredGeneration,
                              retiredPages, retiredStatus, true);
        previousDocument.reset();
        recordRetiredDocument(QStringLiteral("after"), retiredGeneration,
                              retiredPages, retiredStatus, false);
    }
    if (m_content)
    {
        record(QStringLiteral("primary-stream-release"), QStringLiteral("before"));
        m_content.reset();
        record(QStringLiteral("primary-stream-release"), QStringLiteral("after"));
    }
    if (recreate)
    {
        if (!m_document)
        {
            createDocument();
        }
        if (!m_view)
        {
            createView();
        }
        if (timedOut)
        {
            record(QStringLiteral("qpdf-wrapper-recreate"), QStringLiteral("after"));
        }
    }
    record(QStringLiteral("qpdf-generation-release"), QStringLiteral("after"));
    m_pageSelectionCount = 0;
    m_releasing = false;
}

bool QtPdfViewerSession::waitForCloseCompletion()
{
    if (m_closeWaiter)
    {
        return m_closeWaiter();
    }
    if (isTerminalCloseStatus(status()))
    {
        return true;
    }
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(m_document.get(), &QPdfDocument::statusChanged, &loop,
            [&loop](QPdfDocument::Status value) {
        if (isTerminalCloseStatus(value))
        {
            loop.quit();
        }
    });
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(5000);
    while (!isTerminalCloseStatus(status()) && timeout.isActive())
    {
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    return isTerminalCloseStatus(status());
}

QPdfView* QtPdfViewerSession::view() const noexcept { return m_view.get(); }
bool QtPdfViewerSession::hasActiveStream() const noexcept
{
    return m_content && m_content->primaryBody.get().isOpen();
}
bool QtPdfViewerSession::isReady() const noexcept
{
    return status() == QPdfDocument::Status::Ready && pageCount() > 0
        && hasActiveStream();
}
QPdfDocument::Status QtPdfViewerSession::status() const noexcept
{
    return m_document ? m_document->status() : QPdfDocument::Status::Null;
}
QPdfDocument::Error QtPdfViewerSession::error() const noexcept
{
    return m_document ? m_document->error() : QPdfDocument::Error::None;
}
int QtPdfViewerSession::pageCount() const noexcept
{
    return m_document ? m_document->pageCount() : 0;
}
QSizeF QtPdfViewerSession::pagePointSize(int pageIndex) const
{
    return isReady() ? m_document->pagePointSize(pageIndex) : QSizeF{};
}
void QtPdfViewerSession::withReadyDocument(
    const std::function<void(QPdfDocument*)>& action
    )
{
    if (!isReady() || !action)
    {
        return;
    }
    ++m_borrowCount;
    record(QStringLiteral("qpdf-print-borrow"), QStringLiteral("before"));
    struct BorrowGuard final
    {
        QtPdfViewerSession& session;
        ~BorrowGuard()
        {
            --session.m_borrowCount;
            session.record(QStringLiteral("qpdf-print-borrow"), QStringLiteral("after"));
        }
    } guard{*this};
    action(m_document.get());
}
void QtPdfViewerSession::setStatusHandler(StatusHandler handler)
{
    m_statusHandler = std::move(handler);
}
void QtPdfViewerSession::setLifecycleObserver(LifecycleObserver observer)
{
    m_observer = std::move(observer);
}
void QtPdfViewerSession::setCloseWaiterForTesting(CloseWaiter waiter)
{
    m_closeWaiter = std::move(waiter);
}
void QtPdfViewerSession::setDocumentLoadForTesting(
    std::function<void(QPdfDocument&)> loader)
{
    m_documentLoadForTesting = std::move(loader);
}
void QtPdfViewerSession::recordRetiredDocument(
    const QString& phase, quint64 generation, int pagesBeforeClose,
    QPdfDocument::Status retiredStatus, bool present) const noexcept
{
    try
    {
        record(QStringLiteral("qpdf-document-destroy"), phase, {
            {QStringLiteral("retired_generation"), static_cast<qint64>(generation)},
            {QStringLiteral("retired_page_count_before_close"), pagesBeforeClose},
            {QStringLiteral("retired_status_at_destruction"), static_cast<int>(retiredStatus)},
            {QStringLiteral("retired_document_present"), present}
        });
    }
    catch (...) { }
}
void QtPdfViewerSession::record(const QString& operation, const QString& phase,
                              const QJsonObject& additionalDetails) const noexcept
{
    try
    {
        QJsonObject details{
            {QStringLiteral("phase"), phase},
            {QStringLiteral("scope_kind"), QStringLiteral("viewer_session")},
            {QStringLiteral("generation"), static_cast<qint64>(m_generation)},
            {QStringLiteral("status"), static_cast<int>(status())},
            {QStringLiteral("error"), static_cast<int>(error())},
            {QStringLiteral("page_count"), pageCount()},
            {QStringLiteral("active_stream"), hasActiveStream()},
            {QStringLiteral("document_present"), bool(m_document)},
            {QStringLiteral("view_present"), bool(m_view)},
            {QStringLiteral("borrow_count"), m_borrowCount},
            {QStringLiteral("page_selection_count"), static_cast<qint64>(m_pageSelectionCount)},
            {QStringLiteral("page_selection_detail_limit"), 32},
            {QStringLiteral("page_selection_details_suppressed"), static_cast<qint64>(
                m_pageSelectionCount > 32 ? m_pageSelectionCount - 32 : 0)},
            {QStringLiteral("render_observation"), QStringLiteral(
                "QPdfView page selection only; internal render completion unavailable")}
        };
        for (auto entry = additionalDetails.begin(); entry != additionalDetails.end(); ++entry)
            details.insert(entry.key(), entry.value());
        if (m_view && m_view->pageNavigator())
        {
            const int page = m_view->pageNavigator()->currentPage();
            details.insert(QStringLiteral("page_index"), page);
            if (m_document && status() == QPdfDocument::Status::Ready
                && page >= 0 && page < pageCount())
            {
                const QSizeF size = m_document->pagePointSize(page);
                details.insert(QStringLiteral("page_width_points"), size.width());
                details.insert(QStringLiteral("page_height_points"), size.height());
            }
        }
        if (m_diagnostics)
            m_diagnostics->record(operation, QStringLiteral("document_viewer"), m_diagnosticId, details);
        else
            recordProcessMemoryDiagnostic(operation, QStringLiteral("document_viewer"), m_diagnosticId, details);
    }
    catch (...)
    {
        qWarning("ClassMngr QtPdf lifecycle diagnostics failed.");
    }
    // The pre-existing observer remains an independent test seam.
    try
    {
        if (m_observer) m_observer(operation, phase);
    }
    catch (...)
    {
        qWarning("ClassMngr QtPdf lifecycle observer failed.");
    }
}

} // namespace ClassMngr::Next::Platform

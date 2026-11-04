#include "next/platform/qt_pdf_viewer_session.h"

#include <QEventLoop>
#include <QPdfView>
#include <QSizePolicy>
#include <QTimer>
#include <QWidget>

#include <utility>

namespace ClassMngr::Next::Platform
{

std::unique_ptr<PdfViewerSession> makeQtPdfViewerSession(
    Resources::ResourceLoader& loader, QWidget* viewParent)
{
    return std::make_unique<QtPdfViewerSession>(loader, viewParent);
}

QtPdfViewerSession::QtPdfViewerSession(
    Resources::ResourceLoader& loader, QWidget* viewParent
    )
    : QObject(viewParent), m_resourcePort(loader), m_viewParent(viewParent)
{
    createDocument();
    createView();
}

QtPdfViewerSession::~QtPdfViewerSession()
{
    m_statusHandler = {};
    releaseConsumers(false);
}

void QtPdfViewerSession::createDocument()
{
    m_document = std::make_unique<QPdfDocument>(this);
    const quint64 generation = m_generation;
    connect(m_document.get(), &QPdfDocument::statusChanged, this,
            [this, generation](QPdfDocument::Status status) {
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
    auto content = m_resourcePort.openPrimaryPdf(reference, sessionLabel);
    if (!content)
    {
        return Domain::Result<bool>::failure(content.error());
    }
    m_content.emplace(std::move(content.value()));
    QIODevice& device = m_content->primaryBody.get().device();
    if (!device.isOpen() || !device.isReadable() || !device.seek(0))
    {
        static_cast<void>(release());
        return Domain::Result<bool>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unable to read the selected PDF.",
            .recoverable = true
        });
    }
    const quint64 generation = m_generation;
    record(QStringLiteral("qpdf-document-load"), QStringLiteral("before"));
    m_document->load(&device);
    record(QStringLiteral("qpdf-document-load"), QStringLiteral("after"));
    // Match the synchronous Ready contract without dispatching Error cleanup
    // until load() has unwound. Duplicate queued Ready events are suppressed.
    if (status() == QPdfDocument::Status::Ready)
    {
        publishStatus(QPdfDocument::Status::Ready, generation);
    }
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
    ++m_generation;
    m_publishedStatus.reset();
    bool timedOut = false;
    if (m_document && status() != QPdfDocument::Status::Null)
    {
        record(QStringLiteral("qpdf-document-close"), QStringLiteral("before"));
        m_document->close();
        timedOut = !waitForNull();
        record(QStringLiteral("qpdf-document-close"), QStringLiteral("after"));
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
        record(QStringLiteral("qpdf-document-destroy"), QStringLiteral("before"));
        previousDocument.reset();
        record(QStringLiteral("qpdf-document-destroy"), QStringLiteral("after"));
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
    m_releasing = false;
}

bool QtPdfViewerSession::waitForNull()
{
    if (m_closeWaiter)
    {
        return m_closeWaiter();
    }
    if (status() == QPdfDocument::Status::Null)
    {
        return true;
    }
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(m_document.get(), &QPdfDocument::statusChanged, &loop,
            [&loop](QPdfDocument::Status value) {
        if (value == QPdfDocument::Status::Null)
        {
            loop.quit();
        }
    });
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(5000);
    while (status() != QPdfDocument::Status::Null && timeout.isActive())
    {
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    return status() == QPdfDocument::Status::Null;
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
    struct BorrowGuard final
    {
        int& count;
        ~BorrowGuard() { --count; }
    } guard{m_borrowCount};
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
void QtPdfViewerSession::record(const QString& operation, const QString& phase) const
{
    if (m_observer)
    {
        m_observer(operation, phase);
    }
}

} // namespace ClassMngr::Next::Platform

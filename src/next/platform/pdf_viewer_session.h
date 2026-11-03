#pragma once

#include "next/domain/document_content_reference.h"
#include "next/domain/operation_result.h"

#include <QPdfDocument>
#include <QSizeF>

#include <functional>
#include <memory>

class QPdfView;
class QWidget;

namespace ClassMngr::Next::Resources { class ResourceLoader; }

namespace ClassMngr::Next::Platform
{

// Qt presentation port for a selected-document session. All document/render
// consumers and the borrowed source belong to the implementation, never the
// page. Application's DocumentContentSession remains a separate state model.
class PdfViewerSession
{
public:
    using StatusHandler = std::function<void(QPdfDocument::Status)>;
    using LifecycleObserver = std::function<void(const QString&, const QString&)>;
    using CloseWaiter = std::function<bool()>;

    virtual ~PdfViewerSession() = default;
    [[nodiscard]] virtual Domain::Result<bool> open(
        const Domain::DocumentContentReference& reference,
        const QString& sessionLabel) = 0;
    // False while a synchronous print callback is borrowing the document.
    [[nodiscard]] virtual bool release() = 0;
    // Borrow for presentation controls only; document ownership stays here.
    [[nodiscard]] virtual QPdfView* view() const noexcept = 0;
    [[nodiscard]] virtual bool hasActiveStream() const noexcept = 0;
    [[nodiscard]] virtual bool isReady() const noexcept = 0;
    [[nodiscard]] virtual QPdfDocument::Status status() const noexcept = 0;
    [[nodiscard]] virtual QPdfDocument::Error error() const noexcept = 0;
    [[nodiscard]] virtual int pageCount() const noexcept = 0;
    [[nodiscard]] virtual QSizeF pagePointSize(int pageIndex) const = 0;
    // Synchronous borrow for selected-document printing. The session owner
    // must remain alive until the callback returns. The callback must not
    // retain the pointer or destroy the owner; reentrant replacement/release
    // is rejected. Production uses a modal print dialog for this borrow.
    virtual void withReadyDocument(const std::function<void(QPdfDocument*)>& action) = 0;
    virtual void setStatusHandler(StatusHandler handler) = 0;
    virtual void setLifecycleObserver(LifecycleObserver observer) = 0;
    virtual void setCloseWaiterForTesting(CloseWaiter waiter) = 0;
};

using PdfViewerSessionFactory = std::function<std::unique_ptr<PdfViewerSession>(
    Resources::ResourceLoader&, QWidget*)>;

[[nodiscard]] std::unique_ptr<PdfViewerSession> makeQtPdfViewerSession(
    Resources::ResourceLoader& loader, QWidget* viewParent);

} // namespace ClassMngr::Next::Platform

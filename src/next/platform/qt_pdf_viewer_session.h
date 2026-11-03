#pragma once

#include "next/platform/document_content_resource_port.h"
#include "next/platform/pdf_viewer_session.h"

#include <QObject>
#include <QPdfDocument>
#include <QSizeF>

#include <functional>
#include <memory>
#include <optional>

class QPdfView;
class QWidget;

namespace ClassMngr::Next::Platform
{

// GUI-thread session adapter. Owns every QtPdf consumer of the borrowed body.
// No PDF is opened until open(); application content state stays outside this
// adapter. The view is borrowed for presentation controls only.
class QtPdfViewerSession final : public QObject, public PdfViewerSession
{
public:
    explicit QtPdfViewerSession(Resources::ResourceLoader& loader, QWidget* viewParent);
    ~QtPdfViewerSession() override;

    [[nodiscard]] Domain::Result<bool> open(
        const Domain::DocumentContentReference& reference,
        const QString& sessionLabel
        ) override;
    [[nodiscard]] bool release() override;
    [[nodiscard]] QPdfView* view() const noexcept override;
    [[nodiscard]] bool hasActiveStream() const noexcept override;
    [[nodiscard]] bool isReady() const noexcept override;
    [[nodiscard]] QPdfDocument::Status status() const noexcept override;
    [[nodiscard]] QPdfDocument::Error error() const noexcept override;
    [[nodiscard]] int pageCount() const noexcept override;
    [[nodiscard]] QSizeF pagePointSize(int pageIndex) const override;

    // The callback must not retain the pointer. Reentrant release/open while
    // a modal print operation is borrowing the document is rejected.
    void withReadyDocument(const std::function<void(QPdfDocument*)>& action) override;
    void setStatusHandler(StatusHandler handler) override;
    void setLifecycleObserver(LifecycleObserver observer) override;
    void setCloseWaiterForTesting(CloseWaiter waiter) override;

private:
    void createDocument();
    void createView();
    void publishStatus(QPdfDocument::Status status, quint64 generation);
    void releaseConsumers(bool recreate);
    [[nodiscard]] bool waitForNull();
    void record(const QString& operation, const QString& phase) const;

    DocumentContentResourcePort m_resourcePort;
    QWidget* m_viewParent;
    std::unique_ptr<QPdfView> m_view;
    std::unique_ptr<QPdfDocument> m_document;
    std::optional<DocumentContentResource> m_content;
    StatusHandler m_statusHandler;
    LifecycleObserver m_observer;
    CloseWaiter m_closeWaiter;
    quint64 m_generation = 0;
    std::optional<QPdfDocument::Status> m_publishedStatus;
    bool m_releasing = false;
    int m_borrowCount = 0;
};

} // namespace ClassMngr::Next::Platform

#pragma once

#include "next/application/document_catalog_projection.h"
#include "next/application/document_content_session.h"
#include "next/platform/document_content_resource_port.h"
#include "next/platform/document_export_resource_port.h"
#include "next/ui/page.h"

#include <QPdfDocument>
#include <QString>

#include <functional>
#include <optional>

class QLabel;
class QIntValidator;
class QLineEdit;
class QPushButton;
class QPdfView;
class QUrl;
class QVBoxLayout;

namespace ClassMngr::Next::Ui
{

// Owns the selected PDF's QPdfDocument, borrowed file stream, and application
// session. Catalog metadata is copied only while a selection is active.
class DocumentViewerPage final : public Page
{
public:
    struct SaveCopyRequest final
    {
        QString targetPath;
        bool openAfterSaving = false;
    };

    using LifecycleObserver = std::function<void(
        const QString& operation,
        const QString& phase
        )>;
    using CloseWaiter = std::function<bool()>;
    using PrintActionHandler = std::function<void(
        QPdfDocument* document,
        const QString& documentName,
        int pageIndex
        )>;
    using SaveCopyRequestProvider = std::function<
        std::optional<SaveCopyRequest>(
            const QString& suggestedPath,
            const QString& suffix
            )>;
    using OpenSavedCopyHandler = std::function<bool(const QUrl&)>;

    explicit DocumentViewerPage(
        const PageContext& context,
        QWidget* parent = nullptr
        );
    ~DocumentViewerPage() override;

    [[nodiscard]] bool openDocument(
        const Domain::DocumentId& documentId,
        const Application::DocumentCatalogProjection& projection
        );
    [[nodiscard]] bool closeDocument();
    void onSuspending() noexcept override;

    [[nodiscard]] Application::DocumentContentSnapshot
        contentSnapshot() const;
    [[nodiscard]] bool hasActivePrimaryStream() const noexcept;
    [[nodiscard]] bool hasLoadedDocument() const noexcept;
    [[nodiscard]] int documentPageCount() const noexcept;
    [[nodiscard]] QPdfDocument::Status documentStatus() const noexcept;
    [[nodiscard]] QString lastErrorMessage() const;
    [[nodiscard]] bool canPrint() const noexcept;
    [[nodiscard]] bool canSaveCopy() const noexcept;

    // Explicit destination entry point used by tests and the Save a Copy
    // action. It writes the optional PDF or Office asset in bounded chunks.
    [[nodiscard]] bool copySelectedExportTo(
        const QString& targetPath,
        QString* errorMessage = nullptr
        ) const;

    void setLifecycleObserver(LifecycleObserver observer);
    void setCloseWaiterForTesting(CloseWaiter waiter);
    void setPrintActionHandlerForTesting(PrintActionHandler handler);
    void setSaveCopyActionsForTesting(
        SaveCopyRequestProvider requestProvider,
        OpenSavedCopyHandler openHandler
        );

private:
    void buildUi();
    void createPdfConsumers();
    void destroyPdfConsumersForCloseTimeout();
    void handleDocumentStatusChanged(QPdfDocument::Status status);
    void failActiveDocument(const QString& message);
    [[nodiscard]] bool releaseActiveContent(bool clearError);
    [[nodiscard]] bool waitForDocumentNull();
    void updatePageControls();
    void updateOutputActions();
    void applyPageInput();
    void applyZoomInput();
    void applyZoom();
    void updateZoomDisplay();
    void fitToWidth();
    void fitToPage();
    void applyCalculatedFit(bool widthOnly);
    void printSelectedDocument();
    void saveSelectedCopy();
    void showError(const QString& message);
    void recordLifecycleBoundary(
        const QString& operation,
        const QString& phase
        ) const;

    Platform::DocumentContentResourcePort m_contentResourcePort;
    Platform::DocumentExportResourcePort m_exportResourcePort;
    Application::DocumentContentSession m_contentSession;
    std::optional<Application::DocumentContentSession::SessionToken>
        m_contentToken;
    std::optional<Platform::DocumentContentResource> m_primaryContent;
    std::optional<Application::DocumentEntryMetadata> m_selectedDocument;
    std::optional<Domain::DocumentId> m_selectedDocumentId;
    QPdfDocument* m_document = nullptr;
    QPdfView* m_view = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_errorLabel = nullptr;
    QLabel* m_pageLabel = nullptr;
    QLabel* m_pageTotalLabel = nullptr;
    QLineEdit* m_pageInput = nullptr;
    QIntValidator* m_pageValidator = nullptr;
    QLineEdit* m_zoomInput = nullptr;
    QPushButton* m_printButton = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QVBoxLayout* m_rootLayout = nullptr;
    qreal m_zoomFactor = 1.0;
    quint64 m_requestSequence = 0;
    bool m_releasing = false;
    LifecycleObserver m_lifecycleObserver;
    CloseWaiter m_closeWaiter;
    PrintActionHandler m_printActionHandler;
    SaveCopyRequestProvider m_saveCopyRequestProvider;
    OpenSavedCopyHandler m_openSavedCopyHandler;
};

}  // namespace ClassMngr::Next::Ui

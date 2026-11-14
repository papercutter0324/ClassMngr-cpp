#pragma once

#include "next/application/default_output_directories_port.h"
#include "next/application/file_dialog_port.h"
#include "next/application/document_catalog_projection.h"
#include "next/application/document_content_session.h"
#include "next/platform/pdf_print_operation.h"
#include "next/platform/pdf_viewer_session.h"
#include "next/platform/document_export_resource_port.h"
#include "next/ui/page.h"
#include "ui/shared/dialogs/dialog_shell.h"

#include <QPdfDocument>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class QLabel;
class QIntValidator;
class QLineEdit;
class QPushButton;
class QPdfView;
class QEvent;
class QUrl;
class QVBoxLayout;
class QWidget;

namespace ClassMngr::Next::Ui
{

// Presents one selected viewer session. The adapter owns all active QtPdf
// consumers and the borrowed body; this page owns application state and controls.
struct DocumentViewerPageActions final
{
    using SaveFileDialogResponse = Application::FileDialogResponse<
        Application::SavedFileDialogSelection>;
    using OpenSavedCopyHandler = std::function<bool(const QUrl&)>;

    std::function<Domain::Result<SaveFileDialogResponse>(
        QWidget*, const Application::SaveFileDialogRequest&)> saveFileDialog;
    std::shared_ptr<Application::DefaultOutputDirectoriesPort>
        defaultOutputDirectoriesPort;
    OpenSavedCopyHandler openSavedCopy;
};

class DocumentViewerPage final : public Page
{
public:
    using LifecycleObserver = std::function<void(
        const QString& operation,
        const QString& phase
        )>;
    using CloseWaiter = std::function<bool()>;

    explicit DocumentViewerPage(
        const PageContext& context,
        QWidget* parent = nullptr,
        Platform::PdfViewerSessionFactory sessionFactory =
            Platform::makeQtPdfViewerSession,
        Platform::PdfPrintOperationFactory printOperationFactory =
            Platform::makeQtPdfPrintOperation,
        DocumentViewerPageActions actions = {}
        );
    ~DocumentViewerPage() override;

    [[nodiscard]] bool openDocument(
        const Domain::DocumentId& documentId,
        const Application::DocumentCatalogProjection& projection
        );
    void refreshLocalizedCatalogMetadata(
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

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void retranslateUi();
    void attachSessionView();
    void handleDocumentStatusChanged(QPdfDocument::Status status);
    void failActiveDocument(const QString& message);
    [[nodiscard]] bool releaseActiveContent(bool clearError);
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

    std::unique_ptr<Platform::PdfViewerSession> m_pdfSession;
    Platform::PdfPrintOperationFactory m_printOperationFactory;
    Platform::DocumentExportResourcePort m_exportResourcePort;
    Application::DocumentContentSession m_contentSession;
    std::optional<Application::DocumentContentSession::SessionToken>
        m_contentToken;
    std::optional<Application::DocumentEntryMetadata> m_selectedDocument;
    std::optional<Domain::DocumentId> m_selectedDocumentId;
    DocumentViewerPageActions m_actions;
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
    QPushButton* m_zoomOutButton = nullptr;
    QPushButton* m_zoomInButton = nullptr;
    QPushButton* m_fitWidthButton = nullptr;
    QPushButton* m_fitPageButton = nullptr;
    QVBoxLayout* m_rootLayout = nullptr;
    qreal m_zoomFactor = 1.0;
    quint64 m_requestSequence = 0;
    bool m_releasing = false;
    LifecycleObserver m_lifecycleObserver;
};

}  // namespace ClassMngr::Next::Ui

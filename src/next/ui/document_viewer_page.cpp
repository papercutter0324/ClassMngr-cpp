#include "next/ui/document_viewer_page.h"

#include "next/application/document_catalog_use_case.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr qreal kZoomStep = 1.2;
constexpr qreal kMinimumZoom = 0.25;
constexpr qreal kMaximumZoom = 3.0;
constexpr qint64 kCopyBufferSize = 1024 * 1024;
constexpr int kCloseStatusTimeoutMilliseconds = 5000;

QString viewerText(const char* source)
{
    // The Next viewer reuses the established Legacy viewer translations while
    // keeping this page independent of the Legacy page implementation.
    return QCoreApplication::translate("PdfViewerPage", source);
}

QString utf8Text(const std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QString suffixFor(const Domain::DocumentContentReference& reference)
{
    const QByteArray bytes(
        reference.value().data(),
        static_cast<qsizetype>(reference.value().size())
        );
    return QFileInfo(QString::fromUtf8(bytes)).suffix().toLower();
}

QString operationLabel(
    const Domain::DocumentId& documentId,
    quint64 requestNumber
    )
{
    return QStringLiteral("DocumentViewer:%1:%2")
        .arg(
            QString::fromStdString(documentId.value()),
            QString::number(requestNumber)
            );
}

QString documentErrorText(const QPdfDocument::Error error)
{
    switch (error)
    {
    case QPdfDocument::Error::None:
        return viewerText("No error.");
    case QPdfDocument::Error::Unknown:
        return viewerText("Unknown error.");
    case QPdfDocument::Error::DataNotYetAvailable:
        return viewerText("The PDF data is not available yet.");
    case QPdfDocument::Error::FileNotFound:
        return viewerText("The file was not found.");
    case QPdfDocument::Error::InvalidFileFormat:
        return viewerText("The file is not a valid PDF.");
    case QPdfDocument::Error::IncorrectPassword:
        return viewerText("The PDF password is incorrect.");
    case QPdfDocument::Error::UnsupportedSecurityScheme:
        return viewerText("The PDF uses an unsupported security scheme.");
    }
    return viewerText("Unknown error.");
}

}  // namespace

DocumentViewerPage::DocumentViewerPage(
    const PageContext& context,
    QWidget* parent
    )
    : Page(context, parent)
    , m_contentResourcePort(context.resourceLoader())
    , m_exportResourcePort(context.resourceLoader())
{
    buildUi();
}

DocumentViewerPage::~DocumentViewerPage()
{
    if (closeDocument())
    {
        return;
    }

    // Keep the borrowed stream alive until every QtPdf consumer is gone if a
    // close timeout prevents the normal Status::Null release boundary.
    if (m_view)
    {
        delete m_view;
        m_view = nullptr;
    }
    if (m_document)
    {
        recordLifecycleBoundary(
            QStringLiteral("qpdf-document-destroy"),
            QStringLiteral("before")
            );
        delete m_document;
        m_document = nullptr;
        recordLifecycleBoundary(
            QStringLiteral("qpdf-document-destroy"),
            QStringLiteral("after")
            );
    }
    if (m_primaryContent)
    {
        recordLifecycleBoundary(
            QStringLiteral("primary-stream-release"),
            QStringLiteral("before")
            );
        m_primaryContent.reset();
        recordLifecycleBoundary(
            QStringLiteral("primary-stream-release"),
            QStringLiteral("after")
            );
    }
    if (m_contentToken)
    {
        recordLifecycleBoundary(
            QStringLiteral("content-session-release"),
            QStringLiteral("before")
            );
        static_cast<void>(m_contentSession.release());
        m_contentToken.reset();
        recordLifecycleBoundary(
            QStringLiteral("content-session-release"),
            QStringLiteral("after")
            );
    }
}

bool DocumentViewerPage::openDocument(
    const Domain::DocumentId& documentId,
    const Application::DocumentCatalogProjection& projection
    )
{
    if (!closeDocument())
    {
        return false;
    }

    Application::DocumentCatalogUseCase useCase(projection, m_contentSession);
    const auto resolved = useCase.resolveDocument(documentId);
    if (!resolved)
    {
        showError(resolved.error().code == Domain::ErrorCode::NotFound
            ? viewerText("The file was not found.")
            : utf8Text(resolved.error().message));
        return false;
    }

    m_selectedDocument = resolved.value();
    m_selectedDocumentId = documentId;
    m_titleLabel->setText(QString::fromUtf8(
        m_selectedDocument->displayName.data(),
        static_cast<qsizetype>(m_selectedDocument->displayName.size())
        ));

    const auto requested = useCase.requestPrimaryContent(documentId);
    if (!requested)
    {
        const QString message = utf8Text(requested.error().message);
        showError(message);
        m_selectedDocument.reset();
        m_selectedDocumentId.reset();
        updateOutputActions();
        return false;
    }
    m_contentToken = requested.value();

    const auto loading = m_contentSession.beginLoading(*m_contentToken);
    if (!loading)
    {
        const QString message = utf8Text(loading.error().message);
        static_cast<void>(releaseActiveContent(true));
        showError(message);
        return false;
    }

    const QString streamLabel = operationLabel(
        documentId,
        ++m_requestSequence
        );
    auto primary = m_contentResourcePort.openPrimaryPdf(
        resolved.value().contentReference,
        streamLabel
        );
    if (!primary)
    {
        const QString message = utf8Text(primary.error().message);
        static_cast<void>(m_contentSession.fail(
            *m_contentToken,
            primary.error()
            ));
        static_cast<void>(releaseActiveContent(true));
        showError(message);
        return false;
    }

    m_primaryContent.emplace(std::move(primary.value()));
    QIODevice& device = m_primaryContent->primaryBody.get().device();
    if (!device.isOpen() || !device.isReadable() || !device.seek(0))
    {
        const QString message = viewerText("Unable to read the selected PDF.");
        static_cast<void>(m_contentSession.fail(
            *m_contentToken,
            Domain::OperationError{
                .code = Domain::ErrorCode::Technical,
                .message = message.toUtf8().toStdString(),
                .recoverable = true
            }
            ));
        static_cast<void>(releaseActiveContent(true));
        showError(message);
        return false;
    }

    m_zoomFactor = 1.0;
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::Custom);
    m_view->setZoomFactor(m_zoomFactor);
    updateZoomDisplay();
    m_errorLabel->clear();
    m_errorLabel->hide();
    updateOutputActions();

    recordLifecycleBoundary(QStringLiteral("qpdf-document-load"), QStringLiteral("before"));
    m_document->load(&device);
    recordLifecycleBoundary(QStringLiteral("qpdf-document-load"), QStringLiteral("after"));
    // QtPdf can complete a fresh stream synchronously. Its statusChanged
    // handler is deferred so an Error event never releases the stream while
    // load() is on the stack; publish Ready now that load() has returned so
    // the content session cannot lag behind the ready document.
    if (m_document->status() == QPdfDocument::Status::Ready)
    {
        handleDocumentStatusChanged(QPdfDocument::Status::Ready);
    }
    updatePageControls();
    updateOutputActions();
    return true;
}

void DocumentViewerPage::refreshLocalizedCatalogMetadata(
    const Application::DocumentCatalogProjection& projection
    )
{
    if (!m_selectedDocumentId)
    {
        return;
    }

    const auto found = std::find_if(
        projection.documents().begin(),
        projection.documents().end(),
        [this](const Application::DocumentEntryMetadata& document) {
            return document.id == *m_selectedDocumentId;
        }
        );
    if (found == projection.documents().end())
    {
        return;
    }

    m_selectedDocument = *found;
    retranslateUi();
}

bool DocumentViewerPage::closeDocument()
{
    const bool released = releaseActiveContent(true);
    if (!released)
    {
        showError(viewerText("The PDF viewer could not close the active document."));
    }
    return released;
}

void DocumentViewerPage::onSuspending() noexcept
{
    static_cast<void>(closeDocument());
}

Application::DocumentContentSnapshot DocumentViewerPage::contentSnapshot() const
{
    return m_contentSession.snapshot();
}

bool DocumentViewerPage::hasActivePrimaryStream() const noexcept
{
    return m_primaryContent.has_value()
        && m_primaryContent->primaryBody.get().isOpen();
}

bool DocumentViewerPage::hasLoadedDocument() const noexcept
{
    return m_document
        && m_document->status() == QPdfDocument::Status::Ready
        && m_document->pageCount() > 0
        && hasActivePrimaryStream();
}

int DocumentViewerPage::documentPageCount() const noexcept
{
    return m_document ? m_document->pageCount() : 0;
}

QPdfDocument::Status DocumentViewerPage::documentStatus() const noexcept
{
    return m_document ? m_document->status() : QPdfDocument::Status::Null;
}

QString DocumentViewerPage::lastErrorMessage() const
{
    return m_errorLabel ? m_errorLabel->text() : QString{};
}

bool DocumentViewerPage::canPrint() const noexcept
{
    return hasLoadedDocument()
        && m_selectedDocument.has_value()
        && m_selectedDocument->printable;
}

bool DocumentViewerPage::canSaveCopy() const noexcept
{
    return hasLoadedDocument()
        && m_selectedDocument.has_value()
        && m_selectedDocument->exportReference.has_value();
}

bool DocumentViewerPage::copySelectedExportTo(
    const QString& targetPath,
    QString* errorMessage
    ) const
{
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return false;
    };

    if (!canSaveCopy())
    {
        return fail(viewerText("Export is not available for this file."));
    }
    if (targetPath.trimmed().isEmpty())
    {
        return fail(viewerText("A destination file is required."));
    }

    const auto& exportReference = *m_selectedDocument->exportReference;
    auto exportStream = m_exportResourcePort.openExport(
        exportReference,
        QStringLiteral("DocumentViewer:copy:%1")
            .arg(QString::fromStdString(m_selectedDocument->id.value()))
        );
    if (!exportStream)
    {
        return fail(utf8Text(exportStream.error().message));
    }

    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.absolutePath().isEmpty()
        && !QDir().mkpath(targetInfo.absolutePath()))
    {
        return fail(viewerText("Unable to create the destination folder:\n%1")
            .arg(targetInfo.absolutePath()));
    }

    QSaveFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly))
    {
        return fail(viewerText("Unable to write the destination file:\n%1")
            .arg(targetPath));
    }

    QIODevice& source = exportStream.value().get().device();
    if (!source.isOpen() || !source.isReadable())
    {
        target.cancelWriting();
        return fail(viewerText("Unable to read the source file:\n%1")
            .arg(QString::fromUtf8(exportReference.value().data(),
                                   static_cast<qsizetype>(exportReference.value().size()))));
    }

    while (true)
    {
        const QByteArray chunk = source.read(kCopyBufferSize);
        if (chunk.isEmpty())
        {
            if (source.atEnd())
            {
                break;
            }
            target.cancelWriting();
            return fail(viewerText("Unable to read the source file:\n%1")
                .arg(QString::fromUtf8(exportReference.value().data(),
                                       static_cast<qsizetype>(exportReference.value().size()))));
        }

        qsizetype written = 0;
        while (written < chunk.size())
        {
            const qint64 amount = target.write(
                chunk.constData() + written,
                chunk.size() - written
                );
            if (amount <= 0)
            {
                target.cancelWriting();
                return fail(viewerText("Unable to write the destination file:\n%1")
                    .arg(targetPath));
            }
            written += static_cast<qsizetype>(amount);
        }
    }

    if (!target.commit())
    {
        return fail(viewerText("Unable to save the file:\n%1").arg(targetPath));
    }
    if (errorMessage)
    {
        errorMessage->clear();
    }
    return true;
}

void DocumentViewerPage::setLifecycleObserver(LifecycleObserver observer)
{
    m_lifecycleObserver = std::move(observer);
}

void DocumentViewerPage::setCloseWaiterForTesting(CloseWaiter waiter)
{
    m_closeWaiter = std::move(waiter);
}

void DocumentViewerPage::setPrintActionHandlerForTesting(
    PrintActionHandler handler
    )
{
    m_printActionHandler = std::move(handler);
}

void DocumentViewerPage::setSaveCopyActionsForTesting(
    SaveCopyRequestProvider requestProvider,
    OpenSavedCopyHandler openHandler
    )
{
    m_saveCopyRequestProvider = std::move(requestProvider);
    m_openSavedCopyHandler = std::move(openHandler);
}

void DocumentViewerPage::buildUi()
{
    setObjectName(QStringLiteral("classMngrNextDocumentViewer"));
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(16, 12, 16, 12);
    m_rootLayout->setSpacing(8);

    auto* const actions = new QHBoxLayout;
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("documentViewerTitle"));
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_printButton = new QPushButton(viewerText("Print"), this);
    m_printButton->setObjectName(QStringLiteral("documentViewerPrint"));
    m_exportButton = new QPushButton(viewerText("Export"), this);
    m_exportButton->setObjectName(QStringLiteral("documentViewerExport"));
    m_closeButton = new QPushButton(
        QCoreApplication::translate("ActionRegistry", "Close"),
        this
        );
    m_closeButton->setObjectName(QStringLiteral("documentViewerClose"));
    actions->addWidget(m_titleLabel, 1);
    actions->addWidget(m_printButton);
    actions->addWidget(m_exportButton);
    actions->addWidget(m_closeButton);
    m_rootLayout->addLayout(actions);

    auto* const controls = new QHBoxLayout;
    m_pageLabel = new QLabel(viewerText("Page:"), this);
    m_pageLabel->setObjectName(QStringLiteral("documentViewerPageLabel"));
    m_pageInput = new QLineEdit(QStringLiteral("0"), this);
    m_pageInput->setObjectName(QStringLiteral("documentViewerPageInput"));
    m_pageInput->setFixedWidth(56);
    m_pageInput->setAlignment(Qt::AlignCenter);
    m_pageInput->setToolTip(viewerText("Go to page"));
    m_pageValidator = new QIntValidator(1, 1, m_pageInput);
    m_pageInput->setValidator(m_pageValidator);
    m_pageTotalLabel = new QLabel(viewerText("of 0"), this);
    m_pageTotalLabel->setObjectName(QStringLiteral("documentViewerPageTotal"));
    m_zoomInput = new QLineEdit(QStringLiteral("100%"), this);
    m_zoomInput->setObjectName(QStringLiteral("documentViewerZoomInput"));
    m_zoomInput->setFixedWidth(76);
    m_zoomInput->setAlignment(Qt::AlignCenter);
    m_zoomInput->setToolTip(viewerText("Zoom:"));
    m_zoomOutButton = new QPushButton(QStringLiteral("−"), this);
    m_zoomOutButton->setObjectName(QStringLiteral("documentViewerZoomOut"));
    m_zoomInButton = new QPushButton(QStringLiteral("+"), this);
    m_zoomInButton->setObjectName(QStringLiteral("documentViewerZoomIn"));
    m_fitWidthButton = new QPushButton(this);
    m_fitWidthButton->setObjectName(QStringLiteral("documentViewerFitWidth"));
    m_fitPageButton = new QPushButton(this);
    m_fitPageButton->setObjectName(QStringLiteral("documentViewerFitPage"));
    controls->addWidget(m_pageLabel);
    controls->addWidget(m_pageInput);
    controls->addWidget(m_pageTotalLabel);
    controls->addSpacing(16);
    controls->addWidget(m_zoomOutButton);
    controls->addWidget(m_zoomInput);
    controls->addWidget(m_zoomInButton);
    controls->addWidget(m_fitWidthButton);
    controls->addWidget(m_fitPageButton);
    controls->addStretch(1);
    m_rootLayout->addLayout(controls);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("documentViewerError"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    m_rootLayout->addWidget(m_errorLabel);
    createPdfConsumers();
    connect(m_pageInput, &QLineEdit::editingFinished,
            this, [this] { applyPageInput(); });
    connect(m_zoomInput, &QLineEdit::editingFinished,
            this, [this] { applyZoomInput(); });
    connect(m_zoomOutButton, &QPushButton::clicked, this, [this] {
        m_zoomFactor /= kZoomStep;
        applyZoom();
    });
    connect(m_zoomInButton, &QPushButton::clicked, this, [this] {
        m_zoomFactor *= kZoomStep;
        applyZoom();
    });
    connect(m_fitWidthButton, &QPushButton::clicked, this, [this] { fitToWidth(); });
    connect(m_fitPageButton, &QPushButton::clicked, this, [this] { fitToPage(); });
    connect(m_printButton, &QPushButton::clicked,
            this, [this] { printSelectedDocument(); });
    connect(m_exportButton, &QPushButton::clicked,
            this, [this] { saveSelectedCopy(); });
    connect(m_closeButton, &QPushButton::clicked,
            this, [this] { static_cast<void>(closeDocument()); });

    retranslateUi();
}

void DocumentViewerPage::changeEvent(QEvent* event)
{
    Page::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
}

void DocumentViewerPage::retranslateUi()
{
    if (m_titleLabel)
    {
        if (m_selectedDocument)
        {
            m_titleLabel->setText(QString::fromUtf8(
                m_selectedDocument->displayName.data(),
                static_cast<qsizetype>(m_selectedDocument->displayName.size())
                ));
        }
        else
        {
            m_titleLabel->clear();
        }
    }
    if (m_printButton)
    {
        m_printButton->setText(viewerText("Print"));
        m_printButton->setToolTip(viewerText("Print this file"));
    }
    if (m_exportButton)
    {
        m_exportButton->setText(viewerText("Export"));
        m_exportButton->setToolTip(viewerText("Export this file"));
    }
    if (m_closeButton)
    {
        m_closeButton->setText(
            QCoreApplication::translate("ActionRegistry", "Close")
            );
    }
    if (m_pageLabel)
    {
        m_pageLabel->setText(viewerText("Page:"));
    }
    if (m_pageInput)
    {
        m_pageInput->setToolTip(viewerText("Go to page"));
    }
    if (m_zoomInput)
    {
        m_zoomInput->setToolTip(viewerText("Zoom:"));
    }
    if (m_zoomOutButton)
    {
        m_zoomOutButton->setToolTip(viewerText("Zoom out"));
    }
    if (m_zoomInButton)
    {
        m_zoomInButton->setToolTip(viewerText("Zoom in"));
    }
    if (m_fitWidthButton)
    {
        m_fitWidthButton->setText(viewerText("Fit Width"));
    }
    if (m_fitPageButton)
    {
        m_fitPageButton->setText(viewerText("Fit Page"));
    }
    updatePageControls();
    updateZoomDisplay();
    updateOutputActions();
}

void DocumentViewerPage::createPdfConsumers()
{
    if (m_document || m_view || !m_rootLayout)
    {
        return;
    }
    m_document = new QPdfDocument(this);
    m_view = new QPdfView(this);
    m_view->setObjectName(QStringLiteral("documentViewerPdfView"));
    m_view->setDocument(m_document);
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::Custom);
    m_view->setZoomFactor(m_zoomFactor);
    m_view->setPageSpacing(8);
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_rootLayout->addWidget(m_view, 1);

    connect(m_document, &QPdfDocument::statusChanged, this,
            [this](const QPdfDocument::Status status) {
        // Deferring prevents an error status emitted from load() from closing
        // the borrowed stream while QPdfDocument::load is still on the stack.
        QTimer::singleShot(0, this, [this, status] {
            handleDocumentStatusChanged(status);
        });
    });
    if (m_view->pageNavigator())
    {
        connect(
            m_view->pageNavigator(),
            &QPdfPageNavigator::currentPageChanged,
            this,
            [this](int) { updatePageControls(); }
            );
    }
}

void DocumentViewerPage::destroyPdfConsumersForCloseTimeout()
{
    if (m_view)
    {
        recordLifecycleBoundary(
            QStringLiteral("qpdf-view-destroy"),
            QStringLiteral("before")
            );
        m_rootLayout->removeWidget(m_view);
        delete m_view;
        m_view = nullptr;
        recordLifecycleBoundary(
            QStringLiteral("qpdf-view-destroy"),
            QStringLiteral("after")
            );
    }
    if (m_document)
    {
        recordLifecycleBoundary(
            QStringLiteral("qpdf-document-destroy"),
            QStringLiteral("before")
            );
        delete m_document;
        m_document = nullptr;
        recordLifecycleBoundary(
            QStringLiteral("qpdf-document-destroy"),
            QStringLiteral("after")
            );
    }
}

void DocumentViewerPage::handleDocumentStatusChanged(
    const QPdfDocument::Status status
    )
{
    if (m_releasing || !m_document || status != m_document->status())
    {
        return;
    }

    if (status == QPdfDocument::Status::Ready)
    {
        if (m_document->pageCount() <= 0)
        {
            failActiveDocument(viewerText("The selected PDF has no pages."));
            return;
        }
        if (m_contentToken
            && m_contentSession.snapshot().phase()
                != Application::DocumentContentPhase::Ready)
        {
            const auto ready = m_contentSession.markReady(*m_contentToken);
            if (!ready)
            {
                failActiveDocument(utf8Text(ready.error().message));
                return;
            }
        }
        m_errorLabel->clear();
        m_errorLabel->hide();
        m_view->setPageMode(QPdfView::PageMode::MultiPage);
        if (m_view->pageNavigator()
            && m_view->pageNavigator()->currentPage() != 0)
        {
            m_view->pageNavigator()->jump(0, {}, 1.0);
        }
        m_zoomFactor = 1.0;
        m_view->setZoomMode(QPdfView::ZoomMode::Custom);
        m_view->setZoomFactor(m_zoomFactor);
        updateZoomDisplay();
        updatePageControls();
        updateOutputActions();
        return;
    }

    if (status == QPdfDocument::Status::Error)
    {
        failActiveDocument(
            viewerText("Failed to load PDF: %1")
                .arg(documentErrorText(m_document->error()))
            );
        return;
    }

    updatePageControls();
    updateOutputActions();
}

void DocumentViewerPage::failActiveDocument(const QString& message)
{
    if (m_contentToken)
    {
        static_cast<void>(m_contentSession.fail(
            *m_contentToken,
            Domain::OperationError{
                .code = Domain::ErrorCode::Validation,
                .message = message.toUtf8().toStdString(),
                .recoverable = false
            }
            ));
    }
    if (!releaseActiveContent(false))
    {
        showError(message + QLatin1Char('\n')
            + viewerText("The PDF viewer could not release its active stream."));
        return;
    }
    showError(message);
}

bool DocumentViewerPage::releaseActiveContent(const bool clearError)
{
    m_releasing = true;
    bool closeTimedOut = false;
    if (m_document && m_document->status() != QPdfDocument::Status::Null)
    {
        recordLifecycleBoundary(
            QStringLiteral("qpdf-document-close"),
            QStringLiteral("before")
            );
        m_document->close();
        const bool closed = waitForDocumentNull();
        recordLifecycleBoundary(
            QStringLiteral("qpdf-document-close"),
            QStringLiteral("after")
            );
        if (!closed)
        {
            closeTimedOut = true;
            destroyPdfConsumersForCloseTimeout();
        }
    }

    if (m_primaryContent)
    {
        recordLifecycleBoundary(
            QStringLiteral("primary-stream-release"),
            QStringLiteral("before")
            );
        m_primaryContent.reset();
        recordLifecycleBoundary(
            QStringLiteral("primary-stream-release"),
            QStringLiteral("after")
            );
    }

    if (m_contentToken)
    {
        recordLifecycleBoundary(
            QStringLiteral("content-session-release"),
            QStringLiteral("before")
            );
        static_cast<void>(m_contentSession.release());
        m_contentToken.reset();
        recordLifecycleBoundary(
            QStringLiteral("content-session-release"),
            QStringLiteral("after")
            );
    }

    if (closeTimedOut)
    {
        createPdfConsumers();
        recordLifecycleBoundary(
            QStringLiteral("qpdf-wrapper-recreate"),
            QStringLiteral("after")
            );
    }
    m_selectedDocument.reset();
    m_selectedDocumentId.reset();
    m_titleLabel->clear();
    m_zoomFactor = 1.0;
    if (m_view)
    {
        m_view->setZoomMode(QPdfView::ZoomMode::Custom);
        m_view->setZoomFactor(m_zoomFactor);
    }
    updateZoomDisplay();
    updatePageControls();
    if (clearError)
    {
        m_errorLabel->clear();
        m_errorLabel->hide();
    }
    m_releasing = false;
    updateOutputActions();
    return true;
}

bool DocumentViewerPage::waitForDocumentNull()
{
    if (m_closeWaiter)
    {
        return m_closeWaiter();
    }
    if (!m_document || m_document->status() == QPdfDocument::Status::Null)
    {
        return true;
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(
        m_document,
        &QPdfDocument::statusChanged,
        &loop,
        [&loop](const QPdfDocument::Status status) {
            if (status == QPdfDocument::Status::Null)
            {
                loop.quit();
            }
        }
        );
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(kCloseStatusTimeoutMilliseconds);
    while (m_document->status() != QPdfDocument::Status::Null
           && timeout.isActive())
    {
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    return m_document->status() == QPdfDocument::Status::Null;
}

void DocumentViewerPage::updatePageControls()
{
    if (!m_document || !m_pageInput || !m_pageTotalLabel)
    {
        return;
    }
    const int pageCount = m_document->pageCount();
    m_pageValidator->setRange(1, std::max(1, pageCount));
    const QSignalBlocker blocker(m_pageInput);
    if (pageCount <= 0 || !m_view || !m_view->pageNavigator())
    {
        m_pageInput->setText(QStringLiteral("0"));
        m_pageInput->setEnabled(false);
        m_pageTotalLabel->setText(viewerText("of 0"));
        return;
    }
    const int pageNumber = std::clamp(
        m_view->pageNavigator()->currentPage() + 1,
        1,
        pageCount
        );
    m_pageInput->setText(QString::number(pageNumber));
    m_pageInput->setEnabled(true);
    m_pageTotalLabel->setText(viewerText("of %1").arg(pageCount));
}

void DocumentViewerPage::updateOutputActions()
{
    if (m_printButton)
    {
        m_printButton->setEnabled(canPrint());
        m_printButton->setVisible(
            !m_selectedDocument.has_value() || m_selectedDocument->printable
            );
    }
    if (m_exportButton)
    {
        m_exportButton->setEnabled(canSaveCopy());
        m_exportButton->setVisible(
            m_selectedDocument.has_value()
            && m_selectedDocument->exportReference.has_value()
            );
    }
    if (m_closeButton)
    {
        m_closeButton->setEnabled(
            m_contentToken.has_value() || m_primaryContent.has_value()
            );
    }
}

void DocumentViewerPage::applyPageInput()
{
    const int pageCount = documentPageCount();
    if (pageCount <= 0 || !m_view || !m_view->pageNavigator())
    {
        updatePageControls();
        return;
    }
    bool ok = false;
    const int requested = m_pageInput->text().trimmed().toInt(&ok);
    if (!ok)
    {
        updatePageControls();
        return;
    }
    const int target = std::clamp(requested, 1, pageCount);
    m_view->pageNavigator()->jump(
        target - 1,
        {},
        m_view->pageNavigator()->currentZoom()
        );
    updatePageControls();
}

void DocumentViewerPage::applyZoomInput()
{
    QString text = m_zoomInput->text();
    text.remove(QLatin1Char('%'));
    bool ok = false;
    const qreal zoom = text.trimmed().toDouble(&ok) / 100.0;
    if (!ok || !std::isfinite(zoom))
    {
        updateZoomDisplay();
        return;
    }
    m_zoomFactor = zoom;
    applyZoom();
}

void DocumentViewerPage::applyZoom()
{
    m_zoomFactor = std::clamp(m_zoomFactor, kMinimumZoom, kMaximumZoom);
    m_view->setZoomMode(QPdfView::ZoomMode::Custom);
    m_view->setZoomFactor(m_zoomFactor);
    updateZoomDisplay();
}

void DocumentViewerPage::updateZoomDisplay()
{
    if (m_zoomInput)
    {
        const QSignalBlocker blocker(m_zoomInput);
        m_zoomInput->setText(QStringLiteral("%1%").arg(qRound(m_zoomFactor * 100.0)));
    }
}

void DocumentViewerPage::fitToWidth()
{
    applyCalculatedFit(true);
}

void DocumentViewerPage::fitToPage()
{
    applyCalculatedFit(false);
}

void DocumentViewerPage::applyCalculatedFit(const bool widthOnly)
{
    if (!m_document || m_document->status() != QPdfDocument::Status::Ready
        || m_document->pageCount() <= 0 || !m_view->viewport())
    {
        return;
    }
    int pageIndex = m_view->pageNavigator()
        ? m_view->pageNavigator()->currentPage()
        : 0;
    pageIndex = std::clamp(pageIndex, 0, m_document->pageCount() - 1);
    const QSizeF pageSize = m_document->pagePointSize(pageIndex);
    if (!pageSize.isValid() || pageSize.isEmpty())
    {
        return;
    }
    const QMargins margins = m_view->documentMargins();
    const QSize viewport = m_view->viewport()->size();
    const qreal availableWidth = viewport.width() - margins.left() - margins.right();
    const qreal availableHeight = viewport.height() - margins.top() - margins.bottom();
    if (availableWidth <= 0.0 || availableHeight <= 0.0)
    {
        return;
    }
    const qreal widthZoom = availableWidth
        / (pageSize.width() * std::max(m_view->logicalDpiX() / 72.0, 0.01));
    const qreal heightZoom = availableHeight
        / (pageSize.height() * std::max(m_view->logicalDpiY() / 72.0, 0.01));
    m_zoomFactor = widthOnly ? widthZoom : std::min(widthZoom, heightZoom);
    applyZoom();
}

void DocumentViewerPage::printSelectedDocument()
{
    if (!canPrint())
    {
        return;
    }
    const int pageIndex = m_view->pageNavigator()
        ? std::clamp(
            m_view->pageNavigator()->currentPage(),
            0,
            m_document->pageCount() - 1
            )
        : 0;
    const QString documentName = m_titleLabel->text();
    if (m_printActionHandler)
    {
        m_printActionHandler(m_document, documentName, pageIndex);
        return;
    }
    const auto result = PdfPrintService::printPdfDocument({
        this,
        m_document,
        documentName,
        pageIndex,
        viewerText("Print File")
    });
    if (result.status == PdfPrintService::Status::Failed)
    {
        QMessageBox::warning(this, viewerText("Print File"), result.message);
    }
}

void DocumentViewerPage::saveSelectedCopy()
{
    if (!canSaveCopy() || !m_selectedDocument->exportReference)
    {
        return;
    }
    const QString suffix = suffixFor(*m_selectedDocument->exportReference);
    const QString fileName = QFileInfo(QString::fromUtf8(
        m_selectedDocument->exportReference->value().data(),
        static_cast<qsizetype>(m_selectedDocument->exportReference->value().size())
        )).fileName();
    QString initialDirectory = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation
        );
    if (initialDirectory.isEmpty())
    {
        initialDirectory = QDir::homePath();
    }

    const QString suggestedPath = QDir(initialDirectory).filePath(fileName);
    SaveCopyRequest request;
    if (m_saveCopyRequestProvider)
    {
        const auto selected = m_saveCopyRequestProvider(suggestedPath, suffix);
        if (!selected)
        {
            return;
        }
        request = *selected;
    }
    else
    {
        QFileDialog dialog(
            this,
            viewerText("Export File"),
            suggestedPath,
            suffix.isEmpty()
                ? QStringLiteral("All Files (*)")
                : QStringLiteral("%1 Files (*.%2);;All Files (*)")
                      .arg(suffix.toUpper(), suffix)
            );
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setDefaultSuffix(suffix);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        auto* const openAfterSaving = new QCheckBox(
            viewerText("Open after saving"),
            &dialog
            );
        if (auto* const grid = qobject_cast<QGridLayout*>(dialog.layout()))
        {
            const int row = grid->rowCount();
            grid->addWidget(
                openAfterSaving,
                row,
                0,
                1,
                std::max(1, grid->columnCount())
                );
        }
        if (dialog.exec() != QDialog::Accepted
            || dialog.selectedFiles().isEmpty())
        {
            return;
        }
        request.targetPath = dialog.selectedFiles().constFirst();
        request.openAfterSaving = openAfterSaving->isChecked();
    }

    QString targetPath = request.targetPath;
    if (!suffix.isEmpty() && QFileInfo(targetPath).suffix().isEmpty())
    {
        targetPath += QLatin1Char('.') + suffix;
    }
    QString error;
    if (!copySelectedExportTo(targetPath, &error))
    {
        QMessageBox::warning(this, viewerText("Export File"), error);
        return;
    }
    if (request.openAfterSaving)
    {
        const QUrl targetUrl = QUrl::fromLocalFile(targetPath);
        const bool opened = m_openSavedCopyHandler
            ? m_openSavedCopyHandler(targetUrl)
            : QDesktopServices::openUrl(targetUrl);
        if (!opened)
        {
            QMessageBox::warning(
                this,
                viewerText("Open File"),
                viewerText("Unable to open the exported file:\n%1").arg(targetPath)
                );
        }
    }
}

void DocumentViewerPage::showError(const QString& message)
{
    m_errorLabel->setText(message);
    m_errorLabel->show();
    updatePageControls();
    updateOutputActions();
}

void DocumentViewerPage::recordLifecycleBoundary(
    const QString& operation,
    const QString& phase
    ) const
{
    if (m_lifecycleObserver)
    {
        m_lifecycleObserver(operation, phase);
    }
}

}  // namespace ClassMngr::Next::Ui

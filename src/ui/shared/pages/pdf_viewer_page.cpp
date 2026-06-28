#include "pdf_viewer_page.h"

#include "core/fontmanager.h"
#include "ui/shared/styles/role_style_registry.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QLabel>
#include <QLineEdit>
#include <QPdfDocument>
#include <QPdfView>
#include <QPushButton>
#include <QSizePolicy>
#include <QtGlobal>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr qreal ZoomStep = 1.2;
constexpr qreal MinimumZoom = 0.25;
constexpr qreal MaximumZoom = 3.0;

QString zoomText(
    qreal zoom
    )
{
    return QStringLiteral("%1%").arg(
        qRound(zoom * 100.0)
        );
}
}

PdfViewerPage::PdfViewerPage(
    QWidget* parent
    )
    : BasePage(parent)
{
    buildUi();
}

bool PdfViewerPage::loadPdf(
    const QString& filePath
    )
{
    m_currentFilePath =
        filePath;

    if (filePath.trimmed().isEmpty())
    {
        m_document->close();
        showStatusMessage(
            tr("No PDF file selected.")
            );
        return false;
    }

    m_document->close();

    const QPdfDocument::Error error =
        m_document->load(filePath);

    if (error != QPdfDocument::Error::None)
    {
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

void PdfViewerPage::zoomIn()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );

    m_currentZoom *=
        ZoomStep;

    applyZoom();
}

void PdfViewerPage::zoomOut()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );

    m_currentZoom /=
        ZoomStep;

    applyZoom();
}

void PdfViewerPage::resetZoom()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );

    m_currentZoom =
        1.0;

    applyZoom();
}

void PdfViewerPage::fitWidth()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::FitToWidth
        );

    m_zoomInput->setText(
        tr("Fit Width")
        );
}

void PdfViewerPage::fitPage()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::FitInView
        );

    m_zoomInput->setText(
        tr("Fit Page")
        );
}

void PdfViewerPage::applyZoomInput()
{
    QString text =
        m_zoomInput->text();

    text.remove(
        QLatin1Char('%')
        );

    bool ok = false;
    const qreal zoom =
        text.trimmed().toDouble(&ok) / 100.0;

    if (!ok)
    {
        updateZoomDisplay();
        return;
    }

    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );

    m_currentZoom =
        zoom;

    applyZoom();
}

void PdfViewerPage::handleDocumentStatusChanged()
{
    if (m_document->status() == QPdfDocument::Status::Ready)
    {
        m_view->setPageMode(
            QPdfView::PageMode::MultiPage
            );

        clearStatusMessage();
        resetZoom();
        return;
    }

    if (m_document->status() == QPdfDocument::Status::Error)
    {
        showStatusMessage(
            tr("Failed to load PDF: %1")
                .arg(documentErrorText())
            );
    }
}

void PdfViewerPage::buildUi()
{
    setProperty(
        "role",
        UiRoles::Default
        );

    m_document =
        new QPdfDocument(this);

    m_view =
        new QPdfView(this);
    m_view->setObjectName(
        QStringLiteral("pdfViewerView")
        );
    m_view->setDocument(
        m_document
        );
    m_view->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    m_statusLabel =
        new QLabel(this);
    m_statusLabel->setObjectName(
        QStringLiteral("pageSubtitle")
        );
    m_statusLabel->setAlignment(
        Qt::AlignCenter
        );
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setFont(
        FontManager::getUiFont(10)
        );
    m_statusLabel->hide();

    contentLayout()->setContentsMargins(
        16,
        0,
        16,
        0
        );
    contentLayout()->setSpacing(8);
    contentLayout()->addWidget(
        m_statusLabel
        );
    contentLayout()->addWidget(
        m_view
        );

    m_zoomOutButton =
        new QPushButton(
            QStringLiteral("-"),
            this
            );
    m_zoomOutButton->setToolTip(
        tr("Zoom out")
        );

    m_zoomInButton =
        new QPushButton(
            QStringLiteral("+"),
            this
            );
    m_zoomInButton->setToolTip(
        tr("Zoom in")
        );

    for (QPushButton* button : {m_zoomOutButton, m_zoomInButton})
    {
        RoleStyleRegistry::apply(
            button,
            UiRoles::IconButton
            );
        button->setSizePolicy(
            QSizePolicy::Fixed,
            QSizePolicy::Fixed
            );
        button->setFixedSize(
            32,
            28
            );
    }

    m_fitWidthButton =
        new TextFitPushButton(
            tr("Fit Width"),
            this
            );
    m_fitPageButton =
        new TextFitPushButton(
            tr("Fit Page"),
            this
            );

    for (QPushButton* button : {m_fitWidthButton, m_fitPageButton})
    {
        RoleStyleRegistry::apply(
            button,
            UiRoles::ButtonFooter
            );
        button->setSizePolicy(
            QSizePolicy::Preferred,
            QSizePolicy::Preferred
            );
        button->setFixedHeight(32);
    }

    m_zoomLabel =
        new QLabel(
            tr("Zoom:"),
            this
            );
    m_zoomLabel->setObjectName(
        QStringLiteral("pageSubtitle")
        );
    m_zoomLabel->setFont(
        FontManager::getUiFont(10)
        );

    m_zoomInput =
        new QLineEdit(
            zoomText(m_currentZoom),
            this
            );
    RoleStyleRegistry::apply(
        m_zoomInput,
        UiRoles::Input
        );
    m_zoomInput->setFixedWidth(70);
    m_zoomInput->setAlignment(
        Qt::AlignCenter
        );

    bottomLayout()->addStretch();
    bottomLayout()->addWidget(
        m_zoomLabel
        );
    bottomLayout()->addWidget(
        m_zoomOutButton
        );
    bottomLayout()->addWidget(
        m_zoomInput
        );
    bottomLayout()->addWidget(
        m_zoomInButton
        );
    bottomLayout()->addSpacing(20);
    bottomLayout()->addWidget(
        m_fitWidthButton
        );
    bottomLayout()->addWidget(
        m_fitPageButton
        );
    bottomLayout()->addStretch();

    connect(
        m_zoomInButton,
        &QPushButton::clicked,
        this,
        &PdfViewerPage::zoomIn
        );

    connect(
        m_zoomOutButton,
        &QPushButton::clicked,
        this,
        &PdfViewerPage::zoomOut
        );

    connect(
        m_fitWidthButton,
        &QPushButton::clicked,
        this,
        &PdfViewerPage::fitWidth
        );

    connect(
        m_fitPageButton,
        &QPushButton::clicked,
        this,
        &PdfViewerPage::fitPage
        );

    connect(
        m_zoomInput,
        &QLineEdit::returnPressed,
        this,
        &PdfViewerPage::applyZoomInput
        );

    connect(
        m_zoomInput,
        &QLineEdit::editingFinished,
        this,
        &PdfViewerPage::applyZoomInput
        );

    connect(
        m_document,
        &QPdfDocument::statusChanged,
        this,
        &PdfViewerPage::handleDocumentStatusChanged
        );
}

void PdfViewerPage::applyZoom()
{
    m_currentZoom =
        std::clamp(
            m_currentZoom,
            MinimumZoom,
            MaximumZoom
            );

    m_view->setZoomFactor(
        m_currentZoom
        );

    updateZoomDisplay();
}

void PdfViewerPage::updateZoomDisplay()
{
    m_zoomInput->setText(
        zoomText(m_currentZoom)
        );
}

void PdfViewerPage::showStatusMessage(
    const QString& message
    )
{
    m_statusLabel->setText(
        message
        );
    m_statusLabel->show();
}

void PdfViewerPage::clearStatusMessage()
{
    m_statusLabel->clear();
    m_statusLabel->hide();
}

QString PdfViewerPage::documentErrorText() const
{
    return documentErrorText(
        static_cast<int>(
            m_document->error()
            )
        );
}

QString PdfViewerPage::documentErrorText(
    int error
    ) const
{
    switch (static_cast<QPdfDocument::Error>(error))
    {
    case QPdfDocument::Error::None:
        return tr("No error.");

    case QPdfDocument::Error::Unknown:
        return tr("Unknown error.");

    case QPdfDocument::Error::DataNotYetAvailable:
        return tr("The PDF data is not available yet.");

    case QPdfDocument::Error::FileNotFound:
        return tr("The file was not found.");

    case QPdfDocument::Error::InvalidFileFormat:
        return tr("The file is not a valid PDF.");

    case QPdfDocument::Error::IncorrectPassword:
        return tr("The PDF password is incorrect.");

    case QPdfDocument::Error::UnsupportedSecurityScheme:
        return tr("The PDF uses an unsupported security scheme.");
    }

    return tr("Unknown error.");
}

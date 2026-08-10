#include "pdf_viewer_page_p.h"

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
    m_view->setPageSpacing(
        documentPageSpacingPixels(
            m_documentPageSpacing
            )
        );
    applyDocumentViewerBackground();
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

    m_pageLabel =
        new QLabel(
            tr("Page:"),
            this
            );
    m_pageLabel->setObjectName(
        QStringLiteral("pageSubtitle")
        );
    m_pageLabel->setFont(
        FontManager::getUiFont(10)
        );

    m_pageInput =
        new QLineEdit(this);
    RoleStyleRegistry::apply(
        m_pageInput,
        UiRoles::Input
        );
    m_pageInput->setFixedWidth(56);
    m_pageInput->setAlignment(
        Qt::AlignCenter
        );
    m_pageInput->setToolTip(
        tr("Go to page")
        );

    m_pageValidator =
        new QIntValidator(
            1,
            1,
            m_pageInput
            );
    m_pageInput->setValidator(
        m_pageValidator
        );

    m_pageTotalLabel =
        new QLabel(
            tr("of 0"),
            this
            );
    m_pageTotalLabel->setObjectName(
        QStringLiteral("pageSubtitle")
        );
    m_pageTotalLabel->setFont(
        FontManager::getUiFont(10)
        );
    m_pageTotalLabel->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    m_pageTotalLabel->setMinimumWidth(
        QFontMetrics(
            m_pageTotalLabel->font()
            ).horizontalAdvance(
                tr("of %1").arg(
                    QString(
                        PageTotalReservedDigits,
                        QLatin1Char('9')
                        )
                    )
                )
            + PageTotalReservedPadding
        );

    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        0,
        UiConstants::Pages::Margin,
        0
        );
    contentLayout()->setSpacing(
        UiConstants::Pages::Spacing
        );
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
        m_pageLabel
        );
    bottomLayout()->addWidget(
        m_pageInput
        );
    bottomLayout()->addWidget(
        m_pageTotalLabel
        );
    bottomLayout()->addSpacing(20);
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
        m_view,
        &QPdfView::zoomFactorChanged,
        this,
        [this](qreal)
        {
            if (
                m_tearingDown
                || !m_zoomInput
                )
            {
                return;
            }

            m_currentZoom =
                normalizedZoomFactor(
                    m_view->zoomFactor()
                    );
            updateZoomDisplay();
        }
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
        m_pageInput,
        &QLineEdit::returnPressed,
        this,
        &PdfViewerPage::applyPageInput
        );

    connect(
        m_pageInput,
        &QLineEdit::editingFinished,
        this,
        &PdfViewerPage::applyPageInput
        );

    connect(
        m_document,
        &QPdfDocument::statusChanged,
        this,
        &PdfViewerPage::handleDocumentStatusChanged
        );

    connect(
        m_document,
        &QPdfDocument::pageCountChanged,
        this,
        &PdfViewerPage::updatePageDisplay
        );

    connect(
        m_view->pageNavigator(),
        &QPdfPageNavigator::currentPageChanged,
        this,
        &PdfViewerPage::updatePageDisplay
        );

    updatePageDisplay();
    updateDocumentActionButtons();
    applyUiFonts();
}


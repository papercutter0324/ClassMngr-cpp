#include "pdf_viewer_page.h"

#include "core/fontmanager.h"
#include "ui/shared/printing/pdf_print_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/role_style_registry.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QIntValidator>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QObject>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStringList>
#include <QtGlobal>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr qreal ZoomStep = 1.2;
constexpr qreal MinimumZoom = 0.25;
constexpr qreal MaximumZoom = 3.0;
constexpr qsizetype CopyBufferSize = 1024 * 1024;
constexpr int PageTotalReservedDigits = 3;
constexpr int PageTotalReservedPadding = 8;

QString zoomText(
    qreal zoom
    )
{
    return QStringLiteral("%1%").arg(
        std::clamp(
            qRound(zoom * 100.0),
            qRound(MinimumZoom * 100.0),
            qRound(MaximumZoom * 100.0)
            )
        );
}

qreal normalizedZoomFactor(
    qreal zoom
    )
{
    const int zoomPercent =
        std::clamp(
            qRound(zoom * 100.0),
            qRound(MinimumZoom * 100.0),
            qRound(MaximumZoom * 100.0)
            );

    return zoomPercent / 100.0;
}

QString exportFileFilter(
    const QString& suffix
    )
{
    if (suffix.trimmed().isEmpty())
    {
        return QObject::tr("All Files (*)");
    }

    return QObject::tr("%1 Files (*.%2);;All Files (*)")
        .arg(
            suffix.toUpper(),
            suffix
            );
}

QString defaultExportDirectory()
{
    QString directory =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
            );

    if (directory.isEmpty())
    {
        directory =
            QDir::homePath();
    }

    return directory;
}

int exportSuffixRank(
    const QString& suffix
    )
{
    const QString normalizedSuffix =
        suffix.toLower();

    const QStringList preferredSuffixes = {
        QStringLiteral("pptx"),
        QStringLiteral("docx"),
        QStringLiteral("xlsx"),
        QStringLiteral("ppt"),
        QStringLiteral("doc"),
        QStringLiteral("xls")
    };

    const int index =
        preferredSuffixes.indexOf(normalizedSuffix);

    return index >= 0
        ? index
        : preferredSuffixes.size();
}
}

PdfViewerPage::PdfViewerPage(
    QWidget* parent
    )
    : BasePage(parent)
{
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

bool PdfViewerPage::loadPdf(
    const QString& filePath,
    PdfViewerDocumentActions actions
    )
{
    m_currentFilePath =
        filePath;
    m_documentActions =
        actions;

    if (filePath.trimmed().isEmpty())
    {
        m_document->close();
        updateDocumentActionButtons();
        updatePageDisplay();
        showStatusMessage(
            tr("No PDF file selected.")
            );
        return false;
    }

    m_document->close();
    updateDocumentActionButtons();

    const QPdfDocument::Error error =
        m_document->load(filePath);

    if (error != QPdfDocument::Error::None)
    {
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

    applyCalculatedFitZoom();
}

void PdfViewerPage::fitPage()
{
    m_view->setZoomMode(
        QPdfView::ZoomMode::FitInView
        );

    applyCalculatedFitZoom();
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

void PdfViewerPage::exportFile()
{
    const QString sourcePath =
        exportSourcePath();

    if (sourcePath.trimmed().isEmpty())
    {
        QMessageBox::warning(
            this,
            tr("Export File"),
            tr("No file is available to export.")
            );
        return;
    }

    const QFileInfo sourceInfo(sourcePath);

    const QString suggestedPath =
        QDir(
            defaultExportDirectory()
            ).filePath(
                sourceInfo.fileName()
                );

    QFileDialog dialog(
        this,
        tr("Export File"),
        QFileInfo(suggestedPath).absolutePath(),
        exportFileFilter(
            sourceInfo.suffix()
            )
        );
    dialog.setAcceptMode(
        QFileDialog::AcceptSave
        );
    dialog.setFileMode(
        QFileDialog::AnyFile
        );
    dialog.setOption(
        QFileDialog::DontUseNativeDialog,
        true
        );
    dialog.setDefaultSuffix(
        sourceInfo.suffix()
        );
    dialog.selectFile(
        QFileInfo(suggestedPath).fileName()
        );

    auto* openAfterSavingCheck =
        new QCheckBox(
            tr("Open after saving"),
            &dialog
            );

    if (auto* gridLayout = qobject_cast<QGridLayout*>(dialog.layout()))
    {
        gridLayout->addWidget(
            openAfterSavingCheck,
            gridLayout->rowCount(),
            0,
            1,
            gridLayout->columnCount()
            );
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedFiles =
        dialog.selectedFiles();

    if (selectedFiles.isEmpty())
    {
        return;
    }

    QString targetPath =
        selectedFiles.first();

    if (
        !sourceInfo.suffix().isEmpty()
        && QFileInfo(targetPath).suffix().isEmpty()
        )
    {
        targetPath +=
            QStringLiteral(".%1")
                .arg(sourceInfo.suffix());
    }

    QString errorMessage;

    if (
        !copyFileTo(
            sourcePath,
            targetPath,
            &errorMessage
            )
        )
    {
        QMessageBox::warning(
            this,
            tr("Export File"),
            errorMessage
            );
        return;
    }

    if (
        openAfterSavingCheck->isChecked()
        && !QDesktopServices::openUrl(
            QUrl::fromLocalFile(targetPath)
            )
        )
    {
        QMessageBox::warning(
            this,
            tr("Open File"),
            tr("Unable to open the exported file:\n%1")
                .arg(targetPath)
            );
    }
}

void PdfViewerPage::printFile()
{
    int currentPageIndex =
        0;
    const int pageCount =
        m_document ? m_document->pageCount() : 0;

    if (
        pageCount > 0
        &&
        m_view
        && m_view->pageNavigator()
        )
    {
        currentPageIndex =
            std::clamp(
                m_view->pageNavigator()->currentPage(),
                0,
                pageCount - 1
                );
    }

    const PdfPrintService::Result result =
        PdfPrintService::printPdfDocument(
            {
                this,
                m_document,
                m_currentFilePath,
                currentPageIndex,
                tr("Print File")
            }
            );

    if (result.status == PdfPrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            tr("Print File"),
            result.message
            );
        return;
    }

    if (result.status == PdfPrintService::Status::Sent)
    {
        showStatusMessage(
            result.message
            );
    }
}

void PdfViewerPage::applyPageInput()
{
    const int pageCount =
        m_document->pageCount();

    if (pageCount <= 0)
    {
        updatePageDisplay();
        return;
    }

    bool ok = false;
    const int requestedPage =
        m_pageInput->text()
            .trimmed()
            .toInt(&ok);

    if (!ok)
    {
        updatePageDisplay();
        return;
    }

    const int targetPage =
        std::clamp(
            requestedPage,
            1,
            pageCount
            );

    QPdfPageNavigator* navigator =
        m_view->pageNavigator();

    navigator->jump(
        targetPage - 1,
        {},
        navigator->currentZoom()
        );

    updatePageDisplay();
}

void PdfViewerPage::handleDocumentStatusChanged()
{
    if (
        m_tearingDown
        || !m_document
        || !m_view
        )
    {
        return;
    }

    if (m_document->status() == QPdfDocument::Status::Ready)
    {
        m_view->setPageMode(
            QPdfView::PageMode::MultiPage
            );

        clearStatusMessage();
        resetZoom();
        updatePageDisplay();
        updateDocumentActionButtons();
        return;
    }

    if (m_document->status() == QPdfDocument::Status::Error)
    {
        showStatusMessage(
            tr("Failed to load PDF: %1")
                .arg(documentErrorText())
            );
    }

    updatePageDisplay();
    updateDocumentActionButtons();
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

    m_exportButton =
        new TextFitPushButton(
            tr("Export"),
            this
            );
    m_exportButton->setToolTip(
        tr("Export this file")
        );

    m_printButton =
        new TextFitPushButton(
            tr("Print"),
            this
            );
    m_printButton->setToolTip(
        tr("Print this file")
        );

    for (QPushButton* button : {m_exportButton, m_printButton})
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

    const int documentActionButtonWidth =
        std::max(
            m_exportButton->minimumSizeHint().width(),
            m_printButton->minimumSizeHint().width()
            );

    for (QPushButton* button : {m_exportButton, m_printButton})
    {
        button->setMinimumWidth(
            documentActionButtonWidth
            );
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
    bottomLayout()->addSpacing(20);
    bottomLayout()->addWidget(
        m_exportButton
        );
    bottomLayout()->addWidget(
        m_printButton
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
        m_exportButton,
        &QPushButton::clicked,
        this,
        &PdfViewerPage::exportFile
        );

    connect(
        m_printButton,
        &QPushButton::clicked,
        this,
        &PdfViewerPage::printFile
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
}

void PdfViewerPage::applyZoom()
{
    m_currentZoom =
        normalizedZoomFactor(
            m_currentZoom
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

void PdfViewerPage::applyCalculatedFitZoom()
{
    if (
        m_tearingDown
        || !m_view
        || !m_zoomInput
        )
    {
        return;
    }

    m_currentZoom =
        normalizedZoomFactor(
            effectiveZoomFactor()
            );

    m_view->setZoomMode(
        QPdfView::ZoomMode::Custom
        );
    m_view->setZoomFactor(
        m_currentZoom
        );
    updateZoomDisplay();
}

qreal PdfViewerPage::effectiveZoomFactor() const
{
    if (!m_view)
    {
        return m_currentZoom;
    }

    if (m_view->zoomMode() == QPdfView::ZoomMode::Custom)
    {
        return m_view->zoomFactor();
    }

    if (
        !m_document
        || m_document->status() != QPdfDocument::Status::Ready
        || m_document->pageCount() <= 0
        || !m_view->viewport()
        )
    {
        return m_view->zoomFactor();
    }

    int pageIndex =
        0;

    if (m_view->pageNavigator())
    {
        pageIndex =
            std::clamp(
                m_view->pageNavigator()->currentPage(),
                0,
                m_document->pageCount() - 1
                );
    }

    const QSizeF pagePointSize =
        m_document->pagePointSize(pageIndex);

    if (
        !pagePointSize.isValid()
        || pagePointSize.isEmpty()
        )
    {
        return m_view->zoomFactor();
    }

    const QMargins margins =
        m_view->documentMargins();
    const QSize viewportSize =
        m_view->viewport()->size();

    const qreal availableWidth =
        viewportSize.width()
        - margins.left()
        - margins.right();
    const qreal availableHeight =
        viewportSize.height()
        - margins.top()
        - margins.bottom();

    if (
        availableWidth <= 0.0
        || availableHeight <= 0.0
        )
    {
        return m_view->zoomFactor();
    }

    const qreal horizontalPixelsPerPoint =
        std::max(
            m_view->logicalDpiX() / 72.0,
            0.01
            );
    const qreal verticalPixelsPerPoint =
        std::max(
            m_view->logicalDpiY() / 72.0,
            0.01
            );

    const qreal widthZoom =
        availableWidth
        / (pagePointSize.width() * horizontalPixelsPerPoint);
    const qreal heightZoom =
        availableHeight
        / (pagePointSize.height() * verticalPixelsPerPoint);

    if (m_view->zoomMode() == QPdfView::ZoomMode::FitToWidth)
    {
        return std::max(
            widthZoom,
            0.01
            );
    }

    return std::max(
        std::min(
            widthZoom,
            heightZoom
            ),
        0.01
        );
}

void PdfViewerPage::updatePageDisplay()
{
    if (
        m_tearingDown
        || !m_document
        || !m_view
        || !m_view->pageNavigator()
        || !m_pageValidator
        || !m_pageInput
        || !m_pageTotalLabel
        )
    {
        return;
    }

    const int pageCount =
        m_document->pageCount();
    const bool hasPages =
        pageCount > 0;

    m_pageValidator->setRange(
        1,
        std::max(
            1,
            pageCount
            )
        );

    const QSignalBlocker pageInputBlocker(
        m_pageInput
        );

    if (!hasPages)
    {
        m_pageInput->setText(
            QStringLiteral("0")
            );
        m_pageInput->setEnabled(false);
        m_pageTotalLabel->setText(
            tr("of 0")
            );
        return;
    }

    const int currentPage =
        std::clamp(
            m_view->pageNavigator()->currentPage(),
            0,
            pageCount - 1
            ) + 1;

    m_pageInput->setText(
        QString::number(currentPage)
        );
    m_pageInput->setEnabled(true);
    m_pageTotalLabel->setText(
        tr("of %1").arg(pageCount)
        );
}

void PdfViewerPage::updateDocumentActionButtons()
{
    const bool hasFile =
        !m_currentFilePath.trimmed().isEmpty();
    const bool canExport =
        hasFile
        && m_documentActions.exportEnabled;
    const bool canPrint =
        hasFile
        && m_document
        && m_document->status() == QPdfDocument::Status::Ready
        && m_document->pageCount() > 0
        && m_documentActions.printEnabled;

    m_exportButton->setVisible(
        true
        );
    m_exportButton->setEnabled(
        canExport
        );
    m_exportButton->setToolTip(
        canExport
            ? tr("Export this file")
            : tr("Export is not available for this file")
        );

    m_printButton->setVisible(
        true
        );
    m_printButton->setEnabled(
        canPrint
        );
    m_printButton->setToolTip(
        canPrint
            ? tr("Print this file")
            : tr("Print is not available for this file")
        );
}

QString PdfViewerPage::exportSourcePath() const
{
    if (m_currentFilePath.trimmed().isEmpty())
    {
        return QString();
    }

    const QFileInfo pdfInfo(m_currentFilePath);
    const QDir directory(
        pdfInfo.absolutePath()
        );

    if (!directory.exists())
    {
        return m_currentFilePath;
    }

    QList<QFileInfo> alternatives;

    const QFileInfoList files =
        directory.entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot,
            QDir::Name
            );

    for (const QFileInfo& fileInfo : files)
    {
        if (
            fileInfo.completeBaseName().compare(
                pdfInfo.completeBaseName(),
                Qt::CaseInsensitive
                ) != 0
            )
        {
            continue;
        }

        if (
            fileInfo.suffix().compare(
                QStringLiteral("pdf"),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            continue;
        }

        alternatives.append(fileInfo);
    }

    if (alternatives.isEmpty())
    {
        return m_currentFilePath;
    }

    std::sort(
        alternatives.begin(),
        alternatives.end(),
        [](const QFileInfo& left, const QFileInfo& right)
        {
            const int leftRank =
                exportSuffixRank(
                    left.suffix()
                    );
            const int rightRank =
                exportSuffixRank(
                    right.suffix()
                    );

            if (leftRank != rightRank)
            {
                return leftRank < rightRank;
            }

            return left.fileName().localeAwareCompare(
                right.fileName()
                ) < 0;
        }
        );

    return alternatives.first().absoluteFilePath();
}

bool PdfViewerPage::copyFileTo(
    const QString& sourcePath,
    const QString& targetPath,
    QString* errorMessage
    ) const
{
    QFile sourceFile(sourcePath);

    if (!sourceFile.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage =
                tr("Unable to read the source file:\n%1")
                    .arg(sourcePath);
        }

        return false;
    }

    const QFileInfo targetInfo(targetPath);

    if (
        !targetInfo.absolutePath().isEmpty()
        && !QDir().mkpath(targetInfo.absolutePath())
        )
    {
        if (errorMessage)
        {
            *errorMessage =
                tr("Unable to create the destination folder:\n%1")
                    .arg(targetInfo.absolutePath());
        }

        return false;
    }

    QSaveFile targetFile(targetPath);

    if (!targetFile.open(QIODevice::WriteOnly))
    {
        if (errorMessage)
        {
            *errorMessage =
                tr("Unable to write the destination file:\n%1")
                    .arg(targetPath);
        }

        return false;
    }

    while (!sourceFile.atEnd())
    {
        const QByteArray chunk =
            sourceFile.read(CopyBufferSize);

        if (
            chunk.isEmpty()
            && sourceFile.error() != QFile::NoError
            )
        {
            targetFile.cancelWriting();

            if (errorMessage)
            {
                *errorMessage =
                    tr("Unable to read the source file:\n%1")
                        .arg(sourcePath);
            }

            return false;
        }

        if (targetFile.write(chunk) != chunk.size())
        {
            targetFile.cancelWriting();

            if (errorMessage)
            {
                *errorMessage =
                    tr("Unable to write the destination file:\n%1")
                        .arg(targetPath);
            }

            return false;
        }
    }

    if (!targetFile.commit())
    {
        if (errorMessage)
        {
            *errorMessage =
                tr("Unable to save the file:\n%1")
                    .arg(targetPath);
        }

        return false;
    }

    return true;
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

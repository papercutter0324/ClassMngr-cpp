#include "ui/shared/printing/pdf_print_dialog.h"

#include "core/fontmanager.h"
#include "ui/shared/styles/role_style_registry.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPdfDocument>
#ifdef Q_OS_WIN
#include <QPrintDialog>
#endif
#include <QPrintPreviewWidget>
#include <QPrinterInfo>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <utility>

namespace
{
constexpr int OptionsPanelWidth = 320;
constexpr int DialogMinimumWidth = 1050;
constexpr int DialogMinimumHeight = 720;
constexpr int PageRangeAll = 0;
constexpr int PageRangeCustom = 1;
constexpr int ColorModeColor = 0;
constexpr int ColorModeBlackAndWhite = 1;

const QString CustomPagesSample =
    QStringLiteral("e.g. 1-3, 6, 9-11");

QLabel* createOptionLabel(
    const QString& text,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(
            text,
            parent
            );
    label->setObjectName(
        QStringLiteral("pageSubtitle")
        );
    label->setFont(
        FontManager::getUiFont(
            10,
            QFont::Medium
            )
        );
    return label;
}

void addOptionWidget(
    QVBoxLayout* layout,
    QLabel* label,
    QWidget* widget
    )
{
    layout->addWidget(label);
    layout->addWidget(widget);
    layout->addSpacing(10);
}

bool isAllowedPageRangeCharacter(
    QChar character
    )
{
    return character.isDigit()
        || character.isSpace()
        || character == QLatin1Char(',')
        || character == QLatin1Char('-');
}
}

PdfPrintDialog::PdfPrintDialog(
    QWidget* parent,
    QPdfDocument* document,
    const QString& documentPath,
    PdfPrintDialogSupport::RenderFunction renderFunction,
    int currentPageIndex
    )
    : QDialog(parent)
    , m_document(document)
    , m_documentPath(documentPath)
    , m_currentPageIndex(currentPageIndex)
    , m_renderFunction(std::move(renderFunction))
    , m_printResult{
          PdfPrintService::Status::Canceled,
          QString()
      }
    , m_printer(QPrinter::HighResolution)
{
    buildUi();
    populatePrinters();
    connectSignals();
    configurePrinterFromUi();
    updatePrinterCapabilities();
    updatePageRangeControls();
    updateValidation();
    updatePreview();
}

PdfPrintService::Result PdfPrintDialog::printResult() const
{
    return m_printResult;
}

bool PdfPrintDialog::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        watched == m_customPagesEdit
        && event->type() == QEvent::FocusIn
        && m_customPagesEdit->text() == CustomPagesSample
        )
    {
        m_customPagesEdit->clear();
    }

    return QDialog::eventFilter(
        watched,
        event
        );
}

void PdfPrintDialog::buildUi()
{
    setWindowTitle(
        printJobTitle()
        );
    setMinimumSize(
        DialogMinimumWidth,
        DialogMinimumHeight
        );

    auto* rootLayout =
        new QHBoxLayout(this);
    rootLayout->setContentsMargins(
        14,
        14,
        14,
        14
        );
    rootLayout->setSpacing(14);

    auto* optionsPanel =
        new QFrame(this);
    optionsPanel->setObjectName(
        QStringLiteral("printOptionsPanel")
        );
    optionsPanel->setFixedWidth(
        OptionsPanelWidth
        );

    auto* optionsLayout =
        new QVBoxLayout(optionsPanel);
    optionsLayout->setContentsMargins(
        18,
        18,
        18,
        18
        );
    optionsLayout->setSpacing(4);

    auto* titleLabel =
        new QLabel(
            tr("Print"),
            optionsPanel
            );
    titleLabel->setObjectName(
        QStringLiteral("pageTitle")
        );
    titleLabel->setFont(
        FontManager::getUiFont(
            16,
            QFont::DemiBold
            )
        );
    optionsLayout->addWidget(titleLabel);
    optionsLayout->addSpacing(14);

    m_printerCombo =
        new QComboBox(optionsPanel);
    RoleStyleRegistry::apply(
        m_printerCombo,
        UiRoles::Input
        );
    addOptionWidget(
        optionsLayout,
        createOptionLabel(
            tr("Printer"),
            optionsPanel
            ),
        m_printerCombo
        );

    m_pagesCombo =
        new QComboBox(optionsPanel);
    RoleStyleRegistry::apply(
        m_pagesCombo,
        UiRoles::Input
        );
    m_pagesCombo->addItem(
        tr("All"),
        PageRangeAll
        );
    m_pagesCombo->addItem(
        tr("Custom"),
        PageRangeCustom
        );
    optionsLayout->addWidget(
        createOptionLabel(
            tr("Pages"),
            optionsPanel
            )
        );
    optionsLayout->addWidget(m_pagesCombo);

    m_customPagesEdit =
        new QLineEdit(
            CustomPagesSample,
            optionsPanel
            );
    RoleStyleRegistry::apply(
        m_customPagesEdit,
        UiRoles::Input
        );
    m_customPagesEdit->installEventFilter(this);
    m_customPagesEdit->setVisible(false);
    optionsLayout->addWidget(m_customPagesEdit);
    optionsLayout->addSpacing(10);

    m_copiesSpin =
        new QSpinBox(optionsPanel);
    RoleStyleRegistry::apply(
        m_copiesSpin,
        UiRoles::Input
        );
    m_copiesSpin->setRange(
        1,
        999
        );
    m_copiesSpin->setValue(1);
    addOptionWidget(
        optionsLayout,
        createOptionLabel(
            tr("Copies"),
            optionsPanel
            ),
        m_copiesSpin
        );

    m_colorCombo =
        new QComboBox(optionsPanel);
    RoleStyleRegistry::apply(
        m_colorCombo,
        UiRoles::Input
        );
    m_colorCombo->addItem(
        tr("Color"),
        ColorModeColor
        );
    m_colorCombo->addItem(
        tr("Black and White"),
        ColorModeBlackAndWhite
        );
    addOptionWidget(
        optionsLayout,
        createOptionLabel(
            tr("Color"),
            optionsPanel
            ),
        m_colorCombo
        );

    m_paperSizeCombo =
        new QComboBox(optionsPanel);
    RoleStyleRegistry::apply(
        m_paperSizeCombo,
        UiRoles::Input
        );
    m_paperSizeCombo->addItem(
        tr("A4"),
        static_cast<int>(QPageSize::A4)
        );
    m_paperSizeCombo->addItem(
        tr("B4"),
        static_cast<int>(QPageSize::B4)
        );
    m_paperSizeCombo->addItem(
        tr("B5"),
        static_cast<int>(QPageSize::B5)
        );
    addOptionWidget(
        optionsLayout,
        createOptionLabel(
            tr("Paper Size"),
            optionsPanel
            ),
        m_paperSizeCombo
        );

    m_twoSidedCheck =
        new QCheckBox(
            tr("Two-sided"),
            optionsPanel
            );
    m_twoSidedCheck->setChecked(false);
    optionsLayout->addWidget(m_twoSidedCheck);
    optionsLayout->addSpacing(10);

    m_fitToPageCheck =
        new QCheckBox(
            tr("Fit to Page"),
            optionsPanel
            );
    m_fitToPageCheck->setChecked(false);
    optionsLayout->addWidget(m_fitToPageCheck);
    optionsLayout->addSpacing(12);

    m_statusLabel =
        new QLabel(optionsPanel);
    m_statusLabel->setObjectName(
        QStringLiteral("pageSubtitle")
        );
    m_statusLabel->setWordWrap(true);
    optionsLayout->addWidget(m_statusLabel);
    optionsLayout->addStretch();

    auto* buttonLayout =
        new QHBoxLayout;
    buttonLayout->setSpacing(8);

    m_printButton =
        new TextFitPushButton(
            tr("Print"),
            optionsPanel
            );
    RoleStyleRegistry::apply(
        m_printButton,
        UiRoles::Primary
        );

#ifdef Q_OS_WIN
    m_nativePrintButton =
        new TextFitPushButton(
            tr("Use Windows Print Dialog"),
            optionsPanel
            );
    optionsLayout->addWidget(m_nativePrintButton);
    optionsLayout->addSpacing(8);
#endif

    m_cancelButton =
        new TextFitPushButton(
            tr("Cancel"),
            optionsPanel
            );

    buttonLayout->addWidget(m_printButton);
    buttonLayout->addWidget(m_cancelButton);
    optionsLayout->addLayout(buttonLayout);

    m_previewWidget =
        new QPrintPreviewWidget(
            &m_printer,
            this
            );
    m_previewWidget->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    rootLayout->addWidget(optionsPanel);
    rootLayout->addWidget(
        m_previewWidget,
        1
        );
}

void PdfPrintDialog::connectSignals()
{
    connect(
        m_printerCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]
        {
            configurePrinterFromUi();
            updatePrinterCapabilities();
            updateValidation();
            updatePreview();
        }
        );

    connect(
        m_pagesCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]
        {
            updatePageRangeControls();
            updateValidation();
            updatePreview();
        }
        );

    connect(
        m_customPagesEdit,
        &QLineEdit::textChanged,
        this,
        [this]
        {
            updateValidation();
            updatePreview();
        }
        );

    connect(
        m_copiesSpin,
        &QSpinBox::valueChanged,
        this,
        [this]
        {
            configurePrinterFromUi();
            updatePreview();
        }
        );

    connect(
        m_colorCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]
        {
            configurePrinterFromUi();
            updatePreview();
        }
        );

    connect(
        m_paperSizeCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]
        {
            configurePrinterFromUi();
            updatePreview();
        }
        );

    connect(
        m_twoSidedCheck,
        &QCheckBox::toggled,
        this,
        [this]
        {
            configurePrinterFromUi();
            updatePreview();
        }
        );

    connect(
        m_fitToPageCheck,
        &QCheckBox::toggled,
        this,
        [this]
        {
            updatePreview();
        }
        );

    connect(
        m_previewWidget,
        &QPrintPreviewWidget::paintRequested,
        this,
        &PdfPrintDialog::handlePreviewPaintRequested
        );

    connect(
        m_printButton,
        &QPushButton::clicked,
        this,
        &PdfPrintDialog::printDocument
        );

#ifdef Q_OS_WIN
    connect(
        m_nativePrintButton,
        &QPushButton::clicked,
        this,
        &PdfPrintDialog::printWithNativeSystem
        );
#endif

    connect(
        m_cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );
}

void PdfPrintDialog::populatePrinters()
{
    const QList<QPrinterInfo> printers =
        QPrinterInfo::availablePrinters();

    m_hasPrinters =
        !printers.isEmpty();

    if (!m_hasPrinters)
    {
        m_printerCombo->addItem(
            tr("No printers available")
            );
        m_printerCombo->setEnabled(false);
        return;
    }

    const QString defaultPrinterName =
        QPrinterInfo::defaultPrinterName();
    int defaultIndex =
        0;

    for (const QPrinterInfo& printerInfo : printers)
    {
        const int itemIndex =
            m_printerCombo->count();
        m_printerCombo->addItem(
            printerInfo.printerName(),
            printerInfo.printerName()
            );

        if (
            printerInfo.isDefault()
            || printerInfo.printerName() == defaultPrinterName
            )
        {
            defaultIndex =
                itemIndex;
        }
    }

    m_printerCombo->setCurrentIndex(defaultIndex);
}

void PdfPrintDialog::configurePrinterFromUi()
{
    if (m_hasPrinters)
    {
        m_printer.setPrinterName(
            m_printerCombo->currentData().toString()
            );
    }

    m_printer.setDocName(
        printJobTitle()
        );
    m_printer.setCreator(
        QStringLiteral("ClassMngr")
        );
    m_printer.setCopyCount(
        m_copiesSpin->value()
        );
    m_printer.setColorMode(
        m_colorCombo->currentData().toInt() == ColorModeBlackAndWhite
            ? QPrinter::GrayScale
            : QPrinter::Color
        );
    m_printer.setDuplex(
        m_twoSidedCheck->isChecked()
            ? QPrinter::DuplexLongSide
            : QPrinter::DuplexNone
        );

    const auto pageSizeId =
        static_cast<QPageSize::PageSizeId>(
            m_paperSizeCombo->currentData().toInt()
            );
    m_printer.setPageSize(
        QPageSize(pageSizeId)
        );
}

void PdfPrintDialog::updatePrinterCapabilities()
{
    if (!m_hasPrinters)
    {
        m_twoSidedCheck->setChecked(false);
        m_twoSidedCheck->setEnabled(false);
        return;
    }

    const bool supportsDuplex =
        selectedPrinterSupportsDuplex();

    if (!supportsDuplex)
    {
        const QSignalBlocker blocker(m_twoSidedCheck);
        m_twoSidedCheck->setChecked(false);
    }

    m_twoSidedCheck->setEnabled(supportsDuplex);
    configurePrinterFromUi();
}

void PdfPrintDialog::updatePageRangeControls()
{
    const bool customSelected =
        m_pagesCombo->currentData().toInt() == PageRangeCustom;

    if (!customSelected)
    {
        const QSignalBlocker blocker(m_customPagesEdit);
        m_customPagesEdit->setText(CustomPagesSample);
    }

    m_customPagesEdit->setVisible(customSelected);
}

void PdfPrintDialog::updateValidation()
{
#ifdef Q_OS_WIN
    if (m_nativePrintButton)
    {
        m_nativePrintButton->setEnabled(m_hasPrinters);
    }
#endif

    bool ok = false;
    QString errorMessage;
    [[maybe_unused]] const QList<int> selectedPages =
        selectedPageIndexes(
        &ok,
        &errorMessage
        );

    if (!m_hasPrinters)
    {
        m_statusLabel->setText(
            tr("No printers are available.")
            );
        m_printButton->setEnabled(false);
        return;
    }

    if (!ok)
    {
        m_statusLabel->setText(errorMessage);
        m_printButton->setEnabled(false);
        return;
    }

    m_statusLabel->clear();
    m_printButton->setEnabled(true);
}

void PdfPrintDialog::updatePreview()
{
    configurePrinterFromUi();

    if (m_previewWidget)
    {
        m_previewWidget->updatePreview();
    }
}

void PdfPrintDialog::printDocument()
{
    bool ok = false;
    QString errorMessage;
    const PdfPrintDialogSupport::RenderOptions options =
        renderOptions(
            &ok,
            &errorMessage
            );

    if (!ok)
    {
        m_statusLabel->setText(errorMessage);
        m_printButton->setEnabled(false);
        return;
    }

    configurePrinterFromUi();

    if (!m_printer.isValid())
    {
        m_statusLabel->setText(
            tr("No valid printer is available.")
            );
        return;
    }

    m_printResult =
        m_renderFunction(
            m_printer,
            options
            );

    if (m_printResult.status == PdfPrintService::Status::Failed)
    {
        m_statusLabel->setText(m_printResult.message);
        return;
    }

    accept();
}

#ifdef Q_OS_WIN
void PdfPrintDialog::printWithNativeSystem()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::NativeFormat);

    if (m_hasPrinters)
    {
        printer.setPrinterName(
            m_printerCombo->currentData().toString()
            );
    }

    if (!printer.isValid())
    {
        m_statusLabel->setText(
            tr("No valid printer is available.")
            );
        return;
    }

    printer.setDocName(
        printJobTitle()
        );
    printer.setCreator(
        QStringLiteral("ClassMngr")
        );
    printer.setCopyCount(
        m_copiesSpin->value()
        );
    printer.setColorMode(
        m_colorCombo->currentData().toInt() == ColorModeBlackAndWhite
            ? QPrinter::GrayScale
            : QPrinter::Color
        );
    printer.setDuplex(
        m_twoSidedCheck->isChecked()
            ? QPrinter::DuplexLongSide
            : QPrinter::DuplexNone
        );

    const auto pageSizeId =
        static_cast<QPageSize::PageSizeId>(
            m_paperSizeCombo->currentData().toInt()
            );
    printer.setPageSize(
        QPageSize(pageSizeId)
        );

    const int pageCount =
        m_document ? m_document->pageCount() : 0;

    QPrintDialog dialog(
        &printer,
        this
        );
    dialog.setWindowTitle(
        printJobTitle()
        );
    dialog.setMinMax(
        1,
        pageCount
        );
    dialog.setFromTo(
        1,
        pageCount
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    PdfPrintDialogSupport::RenderOptions options;
    options.pageIndexes =
        pageIndexesFromNativePrinterRange(
            printer
            );
    options.grayscale =
        printer.colorMode() == QPrinter::GrayScale;
    options.fitToPage =
        m_fitToPageCheck->isChecked();

    m_printResult =
        m_renderFunction(
            printer,
            options
            );

    if (m_printResult.status == PdfPrintService::Status::Failed)
    {
        m_statusLabel->setText(m_printResult.message);
        return;
    }

    accept();
}
#endif

void PdfPrintDialog::handlePreviewPaintRequested(
    QPrinter* printer
    )
{
    if (!printer)
    {
        return;
    }

    if (!m_hasPrinters)
    {
        return;
    }

    bool ok = false;
    QString errorMessage;
    PdfPrintDialogSupport::RenderOptions options =
        renderOptions(
            &ok,
            &errorMessage
            );

    if (!ok)
    {
        options.pageIndexes =
            allPageIndexes();
    }

    const PdfPrintService::Result result =
        m_renderFunction(
            *printer,
            options
            );

    if (result.status == PdfPrintService::Status::Failed)
    {
        m_statusLabel->setText(result.message);
    }
}

QString PdfPrintDialog::documentDisplayName() const
{
    const QString fileName =
        QFileInfo(m_documentPath).fileName();

    return fileName.trimmed().isEmpty()
        ? tr("PDF Document")
        : fileName;
}

QString PdfPrintDialog::printJobTitle() const
{
    return tr("Print from ClassMngr - %1")
        .arg(
            documentDisplayName()
            );
}

QList<int> PdfPrintDialog::allPageIndexes() const
{
    QList<int> pages;

    const int pageCount =
        m_document ? m_document->pageCount() : 0;
    pages.reserve(pageCount);

    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        pages.append(pageIndex);
    }

    return pages;
}

#ifdef Q_OS_WIN
QList<int> PdfPrintDialog::pageIndexesFromNativePrinterRange(
    const QPrinter& printer
    ) const
{
    const int pageCount =
        m_document ? m_document->pageCount() : 0;

    if (pageCount <= 0)
    {
        return {};
    }

    if (printer.printRange() == QPrinter::CurrentPage)
    {
        return {
            std::clamp(
                m_currentPageIndex,
                0,
                pageCount - 1
                )
        };
    }

    if (printer.printRange() != QPrinter::PageRange)
    {
        return allPageIndexes();
    }

    const int fromPage =
        printer.fromPage();
    const int toPage =
        printer.toPage();

    if (
        fromPage < 1
        || toPage < 1
        || fromPage > toPage
        )
    {
        return allPageIndexes();
    }

    QList<int> pages;
    const int firstPage =
        std::clamp(
            fromPage,
            1,
            pageCount
            );
    const int lastPage =
        std::clamp(
            toPage,
            firstPage,
            pageCount
            );

    pages.reserve(lastPage - firstPage + 1);
    for (int pageNumber = firstPage; pageNumber <= lastPage; ++pageNumber)
    {
        pages.append(pageNumber - 1);
    }

    return pages;
}
#endif

QList<int> PdfPrintDialog::selectedPageIndexes(
    bool* ok,
    QString* errorMessage
    ) const
{
    if (ok)
    {
        *ok =
            true;
    }

    const int pageCount =
        m_document ? m_document->pageCount() : 0;

    if (pageCount <= 0)
    {
        if (ok)
        {
            *ok =
                false;
        }
        if (errorMessage)
        {
            *errorMessage =
                tr("No PDF pages are available.");
        }
        return {};
    }

    if (m_pagesCombo->currentData().toInt() != PageRangeCustom)
    {
        return allPageIndexes();
    }

    const QString text =
        m_customPagesEdit->text().trimmed();

    if (
        text.isEmpty()
        || text == CustomPagesSample
        )
    {
        return allPageIndexes();
    }

    for (QChar character : text)
    {
        if (!isAllowedPageRangeCharacter(character))
        {
            if (ok)
            {
                *ok =
                    false;
            }
            if (errorMessage)
            {
                *errorMessage =
                    tr("Use commas between pages and hyphens only in ranges.");
            }
            return {};
        }
    }

    QList<int> pages;
    const QStringList segments =
        text.split(
            QLatin1Char(',')
            );

    for (const QString& segmentText : segments)
    {
        const QString segment =
            segmentText.trimmed();

        if (segment.isEmpty())
        {
            if (ok)
            {
                *ok =
                    false;
            }
            if (errorMessage)
            {
                *errorMessage =
                    tr("Enter page ranges like 1-3, 6, 9-11.");
            }
            return {};
        }

        const qsizetype hyphenCount =
            segment.count(
                QLatin1Char('-')
                );

        if (hyphenCount > 1)
        {
            if (ok)
            {
                *ok =
                    false;
            }
            if (errorMessage)
            {
                *errorMessage =
                    tr("Use one hyphen per page range.");
            }
            return {};
        }

        if (hyphenCount == 0)
        {
            bool pageOk = false;
            const int pageNumber =
                segment.toInt(&pageOk);

            if (
                !pageOk
                || pageNumber < 1
                || pageNumber > pageCount
                )
            {
                if (ok)
                {
                    *ok =
                        false;
                }
                if (errorMessage)
                {
                    *errorMessage =
                        tr("Use page numbers from 1 to %1.")
                            .arg(pageCount);
                }
                return {};
            }

            pages.append(pageNumber - 1);
            continue;
        }

        const QStringList bounds =
            segment.split(
                QLatin1Char('-')
                );

        bool startOk = false;
        bool endOk = false;
        const int startPage =
            bounds.value(0).trimmed().toInt(&startOk);
        const int endPage =
            bounds.value(1).trimmed().toInt(&endOk);

        if (
            !startOk
            || !endOk
            || startPage < 1
            || endPage < 1
            || startPage > pageCount
            || endPage > pageCount
            || startPage > endPage
            )
        {
            if (ok)
            {
                *ok =
                    false;
            }
            if (errorMessage)
            {
                *errorMessage =
                    tr("Use valid page ranges from 1 to %1.")
                        .arg(pageCount);
            }
            return {};
        }

        for (int pageNumber = startPage; pageNumber <= endPage; ++pageNumber)
        {
            pages.append(pageNumber - 1);
        }
    }

    if (pages.isEmpty())
    {
        return allPageIndexes();
    }

    return pages;
}

PdfPrintDialogSupport::RenderOptions PdfPrintDialog::renderOptions(
    bool* ok,
    QString* errorMessage
    ) const
{
    PdfPrintDialogSupport::RenderOptions options;
    options.pageIndexes =
        selectedPageIndexes(
            ok,
            errorMessage
            );
    options.grayscale =
        m_colorCombo->currentData().toInt() == ColorModeBlackAndWhite;
    options.fitToPage =
        m_fitToPageCheck->isChecked();

    return options;
}

bool PdfPrintDialog::selectedPrinterSupportsDuplex() const
{
    if (!m_hasPrinters)
    {
        return false;
    }

    const QPrinterInfo printerInfo =
        QPrinterInfo::printerInfo(
            m_printerCombo->currentData().toString()
            );

    const QList<QPrinter::DuplexMode> modes =
        printerInfo.supportedDuplexModes();

    return modes.contains(QPrinter::DuplexLongSide)
        || modes.contains(QPrinter::DuplexAuto);
}

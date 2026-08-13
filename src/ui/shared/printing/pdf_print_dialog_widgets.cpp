#include "ui/shared/printing/pdf_print_dialog.h"

#include "core/fontmanager.h"
#include "ui/shared/printing/pdf_print_dialog_internal.h"
#include "ui/shared/styles/role_style_registry.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPrintPreviewWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
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
}

void PdfPrintDialog::buildUi()
{
    using namespace PdfPrintDialogPrivate;

    setWindowTitle(
        printJobTitle()
        );
    setMinimumSize(
        DialogMinimumWidth,
        DialogMinimumHeight
        );

    auto* rootLayout =
        new QHBoxLayout;
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(14);
    contentLayout()->addLayout(rootLayout, 1);

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
            customPagesSample(),
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
        QStringLiteral("A4"),
        static_cast<int>(QPageSize::A4)
        );
    m_paperSizeCombo->addItem(
        QStringLiteral("B4"),
        static_cast<int>(QPageSize::B4)
        );
    m_paperSizeCombo->addItem(
        QStringLiteral("B5"),
        static_cast<int>(QPageSize::B5)
        );
    if (m_preferredPageSize)
    {
        const int preferredPaperIndex =
            m_paperSizeCombo->findData(
                static_cast<int>(*m_preferredPageSize)
                );

        if (preferredPaperIndex >= 0)
        {
            m_paperSizeCombo->setCurrentIndex(
                preferredPaperIndex
                );
        }
    }
    m_paperSizeCombo->setEnabled(
        !m_lockPreferredPageSize
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
    m_twoSidedCheck->setChecked(true);
    optionsLayout->addWidget(m_twoSidedCheck);
    optionsLayout->addSpacing(10);

    m_fitToPageCheck =
        new QCheckBox(
            tr("Fit to Page"),
            optionsPanel
            );
    m_fitToPageCheck->setChecked(m_fitToPageByDefault);
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

void PdfPrintDialog::updatePageRangeControls()
{
    const bool customSelected =
        m_pagesCombo->currentData().toInt()
            == PdfPrintDialogPrivate::PageRangeCustom;

    if (!customSelected)
    {
        const QSignalBlocker blocker(m_customPagesEdit);
        m_customPagesEdit->setText(
            PdfPrintDialogPrivate::customPagesSample()
            );
    }

    m_customPagesEdit->setVisible(customSelected);
}

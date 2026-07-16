#include "features/roster/ui/roster_print_dialog.h"

#include "core/application_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "domain/models/class_info.h"
#include "ui/shared/widgets/marquee_item_delegate.h"
#include "domain/models/teacher.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
constexpr int AllClassesId = 0;
constexpr int CurrentClassId = 1;
constexpr int SelectedClassesId = 2;
constexpr int PortraitOrientationId = 10;
constexpr int LandscapeOrientationId = 11;
constexpr int DialogWidth = 520;
constexpr int DialogExpandedHeight = 620;
constexpr int PreviewHeight = 190;
constexpr int ExtraColumnGridColumns = 3;
constexpr int ExtraColumnVisibleRows = 3;
constexpr int MaximumDialogWidthMultiplier = 3;
constexpr int MaximumDialogWidthDivisor = 2;


int scopeId(
    RosterTemplatePrintService::Scope scope
    )
{
    switch (scope)
    {
    case RosterTemplatePrintService::Scope::CurrentClass:
        return CurrentClassId;

    case RosterTemplatePrintService::Scope::SelectedClasses:
        return SelectedClassesId;

    case RosterTemplatePrintService::Scope::AllClasses:
    default:
        return AllClassesId;
    }
}
}

RosterPrintDialog::RosterPrintDialog(
    ApplicationServices* services,
    int currentClassId,
    RosterTemplatePrintService::Scope defaultScope,
    QWidget* parent
    )
    : QDialog(parent)
    , m_services(services)
    , m_currentClassId(currentClassId)
    , m_defaultScope(defaultScope)
{
    buildUi();
    loadClasses();
    retranslateUi();
    updateTemplateOptionsVisibility();
    updateClassListVisibility();
}

RosterPrintDialog::Action RosterPrintDialog::selectedAction() const
{
    return m_selectedAction;
}

QString RosterPrintDialog::selectedSavePath() const
{
    return m_selectedSavePath;
}

RosterTemplatePrintService::Scope RosterPrintDialog::selectedScope() const
{
    if (!m_scopeGroup)
    {
        return m_defaultScope;
    }

    switch (m_scopeGroup->checkedId())
    {
    case CurrentClassId:
        return RosterTemplatePrintService::Scope::CurrentClass;

    case SelectedClassesId:
        return RosterTemplatePrintService::Scope::SelectedClasses;

    case AllClassesId:
    default:
        return RosterTemplatePrintService::Scope::AllClasses;
    }
}

RosterTemplatePrintService::TemplateId RosterPrintDialog::selectedTemplateId() const
{
    if (m_hasFinalSelection)
    {
        return m_finalTemplateId;
    }

    if (!m_templateCombo)
    {
        return RosterTemplatePrintService::TemplateId::ByDay;
    }

    return static_cast<RosterTemplatePrintService::TemplateId>(
        m_templateCombo->currentData().toInt()
        );
}

QStringList RosterPrintDialog::selectedExtraColumns() const
{
    if (m_hasFinalSelection)
    {
        return m_finalExtraColumns;
    }

    QStringList columns;

    if (
        selectedTemplateId()
            != RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
        )
    {
        return columns;
    }

    for (const QCheckBox* checkBox : m_extraColumnChecks)
    {
        if (checkBox && checkBox->isChecked())
        {
            columns.append(checkBox->text());
        }
    }

    return columns;
}

QPageLayout::Orientation
RosterPrintDialog::selectedPerClassExtraInfoOrientation() const
{
    if (m_hasFinalSelection)
    {
        return m_finalPerClassExtraInfoOrientation;
    }

    if (!m_extraInfoOrientationGroup)
    {
        return QPageLayout::Portrait;
    }

    return m_extraInfoOrientationGroup->checkedId() == LandscapeOrientationId
        ? QPageLayout::Landscape
        : QPageLayout::Portrait;
}

QList<int> RosterPrintDialog::selectedClassIds() const
{
    QList<int> ids;

    if (!m_classList)
    {
        return ids;
    }

    for (int index = 0; index < m_classList->count(); ++index)
    {
        const QListWidgetItem* item =
            m_classList->item(index);

        if (item && item->checkState() == Qt::Checked)
        {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }

    return ids;
}

void RosterPrintDialog::resizeEvent(
    QResizeEvent* event
    )
{
    QDialog::resizeEvent(event);
    QTimer::singleShot(
        0,
        this,
        &RosterPrintDialog::updatePreview
        );
}

void RosterPrintDialog::acceptPrint()
{
    captureFinalSelection();
    m_selectedAction =
        Action::Print;
    m_selectedSavePath.clear();

    accept();
}

void RosterPrintDialog::chooseSavePath()
{
    QFileDialog dialog(
        this,
        tr("Save Rosters As"),
        QString(),
        tr("PDF Documents (*.pdf)")
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
        QStringLiteral("pdf")
        );
    dialog.selectFile(
        QStringLiteral("Rosters.pdf")
        );

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

    QString savePath =
        selectedFiles.first();

    if (QFileInfo(savePath).suffix().isEmpty())
    {
        savePath +=
            QStringLiteral(".pdf");
    }

    m_selectedAction =
        Action::SaveAs;
    m_selectedSavePath =
        savePath;

    captureFinalSelection();

    accept();
}

void RosterPrintDialog::captureFinalSelection()
{
    // The PDF request is assembled after exec() returns. Preserve the exact
    // column selection that was visible when the user chose Print or Save.
    m_finalTemplateId =
        selectedTemplateId();
    m_finalExtraColumns =
        selectedExtraColumns();
    m_finalPerClassExtraInfoOrientation =
        selectedPerClassExtraInfoOrientation();
    m_hasFinalSelection = true;
}

void RosterPrintDialog::updateClassListVisibility()
{
    if (!m_classList)
    {
        return;
    }

    const bool showClassList =
        selectedScope() == RosterTemplatePrintService::Scope::SelectedClasses;
    const int currentWidth =
        width();

    m_classList->setVisible(showClassList);
    m_classList->setEnabled(showClassList);

    if (QWidget* parent = m_classList->parentWidget())
    {
        if (QLayout* parentLayout = parent->layout())
        {
            parentLayout->invalidate();
            parentLayout->activate();
        }
    }

    if (QLayout* rootLayout = layout())
    {
        rootLayout->invalidate();
        rootLayout->activate();
    }

    if (auto* rootBox = static_cast<QBoxLayout*>(layout()))
    {
        if (m_previewLabel && m_previewLabel->parentWidget())
        {
            rootBox->setStretchFactor(
                m_previewLabel->parentWidget(),
                1
                );
        }

        if (m_classList->parentWidget())
        {
            rootBox->setStretchFactor(
                m_classList->parentWidget(),
                showClassList ? 1 : 0
                );
        }
    }

    if (showClassList)
    {
        resize(
            currentWidth,
            qMax(height(), DialogExpandedHeight)
            );
    }
    else
    {
        adjustSize();
    }

    if (
        selectedTemplateId()
        == RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
        )
    {
        updateExtraInfoColumns();
    }
    else
    {
        updatePreview();
    }
}

void RosterPrintDialog::updateTemplateOptionsVisibility()
{
    const auto templateId = selectedTemplateId();
    const bool showExtraInfoOptions =
        templateId
        == RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo;

    if (m_extraInfoOptionsGroup)
    {
        m_extraInfoOptionsGroup->setVisible(showExtraInfoOptions);
        m_extraInfoOptionsGroup->setEnabled(showExtraInfoOptions);
    }

    if (m_extraInfoOrientationGroup)
    {
        if (auto* portrait =
                m_extraInfoOrientationGroup->button(PortraitOrientationId))
        {
            const bool supported =
                templateId
                != RosterTemplatePrintService::TemplateId::ByDay;
            portrait->setVisible(supported);
            portrait->setEnabled(supported);

            if (
                supported
                && templateId
                    == RosterTemplatePrintService::TemplateId::Daily
                )
            {
                portrait->setChecked(true);
            }
        }

        if (auto* landscape =
                m_extraInfoOrientationGroup->button(LandscapeOrientationId))
        {
            const bool supported =
                templateId
                != RosterTemplatePrintService::TemplateId::Daily;
            landscape->setVisible(supported);
            landscape->setEnabled(supported);

            if (
                supported
                && templateId
                    == RosterTemplatePrintService::TemplateId::ByDay
                )
            {
                landscape->setChecked(true);
            }
        }
    }

    if (showExtraInfoOptions)
    {
        updateExtraInfoColumns();
    }
    else
    {
        setMinimumHeight(m_baseMinimumHeight);
        updatePreview();
    }
}

void RosterPrintDialog::updateExtraInfoColumns()
{
    if (
        selectedTemplateId()
            != RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
        ||
        !m_extraColumnGridLayout
        || !m_extraColumnOptionsWidget
        || !m_services
        || !m_services->dataService()
        )
    {
        updatePreview();
        return;
    }

    QStringList previouslyChecked =
        selectedExtraColumns();

    const QList<Classroom> classes =
        m_services->dataService()->getClasses();
    const QList<int> classIds =
        RosterTemplatePrintService::resolveClassIds(
            selectedScope(),
            m_currentClassId,
            selectedClassIds(),
            classes
            );
    QList<RosterTemplatePrintService::RosterClassData> rosterClasses;
    rosterClasses.reserve(classIds.size());

    for (int classId : classIds)
    {
        RosterTemplatePrintService::RosterClassData data;
        data.roster =
            m_services->dataService()->loadRoster(classId);
        rosterClasses.append(data);
    }

    const QStringList columns =
        RosterTemplatePrintService::availablePerClassExtraInfoColumns(
            rosterClasses
            );

    while (QLayoutItem* item = m_extraColumnGridLayout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    m_extraColumnChecks.clear();

    QList<int> columnWidths(ExtraColumnGridColumns, 0);

    for (int column = 0; column < ExtraColumnGridColumns; ++column)
    {
        m_extraColumnGridLayout->setColumnMinimumWidth(column, 0);
    }

    for (int index = 0; index < columns.size(); ++index)
    {
        const QString& column = columns.at(index);
        auto* checkBox = new QCheckBox(column);
        checkBox->setToolTip(column);
        checkBox->setChecked(
            previouslyChecked.contains(column, Qt::CaseInsensitive)
            );

        const int gridColumn = index % ExtraColumnGridColumns;
        columnWidths[gridColumn] = qMax(
            columnWidths.at(gridColumn),
            checkBox->sizeHint().width()
            );

        m_extraColumnGridLayout->addWidget(
            checkBox,
            index / ExtraColumnGridColumns,
            gridColumn
            );
        m_extraColumnChecks.append(checkBox);

        connect(
            checkBox,
            &QCheckBox::toggled,
            this,
            &RosterPrintDialog::updateExtraInfoSelectionLimits
            );
    }

    int optionsWidth = 0;

    for (int column = 0; column < ExtraColumnGridColumns; ++column)
    {
        m_extraColumnGridLayout->setColumnMinimumWidth(
            column,
            columnWidths.at(column)
            );
        optionsWidth += columnWidths.at(column);
    }

    optionsWidth +=
        (ExtraColumnGridColumns - 1)
        * m_extraColumnGridLayout->horizontalSpacing();
    m_extraColumnOptionsWidget->setMinimumWidth(optionsWidth);
    m_extraColumnOptionsWidget->resize(
        optionsWidth,
        m_extraColumnOptionsWidget->minimumHeight()
        );

    updateExtraInfoSelectionLimits();
    resizeForExtraInfoOptions();
    updatePreview();
}

void RosterPrintDialog::updateExtraInfoSelectionLimits()
{
    if (!m_extraColumnGridLayout)
    {
        return;
    }

    const int maxSelected =
        RosterTemplatePrintService::perClassExtraInfoMaxExtraColumns(
            selectedPerClassExtraInfoOrientation()
            );
    int selectedCount = 0;

    {
        for (QCheckBox* checkBox : m_extraColumnChecks)
        {
            if (!checkBox)
            {
                continue;
            }

            if (checkBox->isChecked())
            {
                ++selectedCount;

                if (selectedCount > maxSelected)
                {
                    const QSignalBlocker blocker(checkBox);
                    checkBox->setChecked(false);
                    --selectedCount;
                }
            }
        }

        const bool atLimit =
            selectedCount >= maxSelected;

        for (QCheckBox* checkBox : m_extraColumnChecks)
        {
            if (!checkBox)
            {
                continue;
            }

            if (
                atLimit
                && !checkBox->isChecked()
                )
            {
                checkBox->setEnabled(false);
            }
            else
            {
                checkBox->setEnabled(true);
            }
        }
    }

    if (m_extraInfoSelectionCountLabel)
    {
        m_extraInfoSelectionCountLabel->setText(
            tr("%1 of %2 selected").arg(selectedCount).arg(maxSelected)
            );
    }

    updatePreview();
}

void RosterPrintDialog::updatePreview()
{
    if (!m_previewLabel)
    {
        return;
    }

    const QSize previewSize(
        qMax(
            1,
            m_previewLabel->width() - 16
            ),
        qMax(
            1,
            m_previewLabel->height() - 16
            )
        );
    const bool livePreview =
        m_livePreviewCheckBox
        && m_livePreviewCheckBox->isChecked();

    QImage preview;
    QString errorMessage;
    RosterTemplatePrintService::Request request;
    request.parent = this;
    request.services = m_services;
    request.currentClassId = m_currentClassId;
    request.scope = selectedScope();
    request.selectedClassIds = selectedClassIds();
    request.templateId = selectedTemplateId();
    request.selectedExtraColumns = selectedExtraColumns();
    request.perClassExtraInfoOrientation =
        selectedPerClassExtraInfoOrientation();

    if (livePreview)
    {
        preview =
            RosterTemplatePrintService::renderTemplatePreview(
                request,
                previewSize,
                true,
                &errorMessage
                );
    }

    if (preview.isNull())
    {
        preview =
            RosterTemplatePrintService::renderTemplatePreview(
                request,
                previewSize,
                false
                );
    }

    if (preview.isNull())
    {
        m_previewLabel->clear();
    }
    else
    {
        m_previewLabel->setPixmap(
            QPixmap::fromImage(preview)
            );
    }

    if (m_previewStatusLabel)
    {
        m_previewStatusLabel->setVisible(
            livePreview
            && !errorMessage.trimmed().isEmpty()
            );
        m_previewStatusLabel->setText(
            errorMessage.trimmed().isEmpty()
                ? QString()
                : tr("Live preview unavailable.")
            );
        m_previewStatusLabel->setToolTip(errorMessage);
    }
}

void RosterPrintDialog::buildUi()
{
    setModal(true);
    setMinimumWidth(DialogWidth);
    setMaximumWidth(
        DialogWidth * MaximumDialogWidthMultiplier
        / MaximumDialogWidthDivisor
        );
    resize(DialogWidth, DialogExpandedHeight);

    auto* rootLayout =
        new QVBoxLayout(this);
    rootLayout->setSpacing(12);

    auto* templateGroupBox =
        new QGroupBox(this);
    auto* templateLayout =
        new QVBoxLayout(templateGroupBox);

    m_templateCombo =
        new NoWheelComboBox(templateGroupBox);
    m_templateCombo->setObjectName(QStringLiteral("templateCombo"));

    for (RosterTemplatePrintService::TemplateId templateId
         : RosterTemplatePrintService::availableTemplateIds())
    {
        m_templateCombo->addItem(
            RosterTemplatePrintService::templateDisplayName(templateId),
            static_cast<int>(templateId)
            );
    }

    templateLayout->addWidget(m_templateCombo);
    templateLayout->addSpacing(24);

    auto* pageLayoutOptionsWidget = new QFrame(templateGroupBox);
    pageLayoutOptionsWidget->setObjectName(
        QStringLiteral("pageLayoutOptionsGroup")
        );
    pageLayoutOptionsWidget->setFrameShape(QFrame::StyledPanel);
    auto* pageLayoutOptionsLayout = new QVBoxLayout(pageLayoutOptionsWidget);
    pageLayoutOptionsLayout->setContentsMargins(12, 12, 12, 24);

    m_pageLayoutLabel = new QLabel(pageLayoutOptionsWidget);

    auto* orientationLayout =
        new QHBoxLayout();
    orientationLayout->setContentsMargins(0, 0, 0, 0);
    orientationLayout->setSpacing(32);
    auto* portraitRadio =
        new QRadioButton(pageLayoutOptionsWidget);
    auto* landscapeRadio =
        new QRadioButton(pageLayoutOptionsWidget);
    portraitRadio->setObjectName(QStringLiteral("portraitExtraInfoRadio"));
    landscapeRadio->setObjectName(QStringLiteral("landscapeExtraInfoRadio"));

    m_extraInfoOrientationGroup =
        new QButtonGroup(this);
    m_extraInfoOrientationGroup->addButton(
        portraitRadio,
        PortraitOrientationId
        );
    m_extraInfoOrientationGroup->addButton(
        landscapeRadio,
        LandscapeOrientationId
        );
    portraitRadio->setChecked(true);

    orientationLayout->addWidget(portraitRadio);
    orientationLayout->addWidget(landscapeRadio);
    orientationLayout->addStretch(1);
    pageLayoutOptionsLayout->addWidget(m_pageLayoutLabel);
    pageLayoutOptionsLayout->addLayout(orientationLayout);

    auto* extraInfoOptionsFrame = new QFrame(templateGroupBox);
    extraInfoOptionsFrame->setFrameShape(QFrame::StyledPanel);
    m_extraInfoOptionsGroup = extraInfoOptionsFrame;
    m_extraInfoOptionsGroup->setObjectName(QStringLiteral("extraInfoOptionsGroup"));
    auto* extraInfoLayout = new QVBoxLayout(m_extraInfoOptionsGroup);
    extraInfoLayout->setContentsMargins(12, 12, 12, 12);

    auto* extraInfoHeaderLayout = new QHBoxLayout();
    extraInfoHeaderLayout->setContentsMargins(0, 0, 0, 0);

    m_extraInfoColumnsLabel = new QLabel(m_extraInfoOptionsGroup);
    m_extraInfoSelectionCountLabel =
        new QLabel(m_extraInfoOptionsGroup);
    m_extraInfoSelectionCountLabel->setAlignment(
        Qt::AlignRight
        | Qt::AlignVCenter
        );

    extraInfoHeaderLayout->addWidget(m_extraInfoColumnsLabel);
    extraInfoHeaderLayout->addStretch(1);
    extraInfoHeaderLayout->addWidget(m_extraInfoSelectionCountLabel);
    extraInfoLayout->addLayout(extraInfoHeaderLayout);

    m_extraColumnOptionsWidget = new QWidget(m_extraInfoOptionsGroup);
    m_extraColumnOptionsWidget->setObjectName(
        QStringLiteral("extraColumnOptions")
        );
    m_extraColumnGridLayout = new QGridLayout(m_extraColumnOptionsWidget);
    m_extraColumnGridLayout->setContentsMargins(0, 0, 0, 0);
    m_extraColumnGridLayout->setHorizontalSpacing(18);
    m_extraColumnGridLayout->setVerticalSpacing(10);

    const int optionRowHeight = portraitRadio->sizeHint().height();
    const int optionRowSpacing = m_extraColumnGridLayout->verticalSpacing();

    for (int column = 0; column < ExtraColumnGridColumns; ++column)
    {
        m_extraColumnGridLayout->setColumnStretch(column, 1);
    }

    for (int row = 0; row < ExtraColumnVisibleRows; ++row)
    {
        m_extraColumnGridLayout->setRowMinimumHeight(row, optionRowHeight);
    }

    const int optionsHeight =
        ExtraColumnVisibleRows * optionRowHeight
        + (ExtraColumnVisibleRows - 1) * optionRowSpacing;
    m_extraColumnOptionsWidget->setMinimumHeight(optionsHeight);

    m_extraColumnScrollArea = new QScrollArea(m_extraInfoOptionsGroup);
    m_extraColumnScrollArea->setObjectName(
        QStringLiteral("extraColumnScrollArea")
        );
    m_extraColumnScrollArea->setFrameShape(QFrame::NoFrame);
    m_extraColumnScrollArea->setWidgetResizable(false);
    m_extraColumnScrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    m_extraColumnScrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
        );
    m_extraColumnScrollArea->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Minimum
        );
    m_extraColumnScrollArea->setMinimumHeight(
        optionsHeight
        + m_extraColumnScrollArea->style()->pixelMetric(
            QStyle::PM_ScrollBarExtent
            )
        );
    m_extraColumnScrollArea->setWidget(m_extraColumnOptionsWidget);

    extraInfoLayout->addWidget(m_extraColumnScrollArea);
    m_extraInfoOptionsGroup->setVisible(false);

    auto* previewSection = new QFrame(templateGroupBox);
    previewSection->setObjectName(QStringLiteral("rosterPreviewSection"));
    previewSection->setFrameShape(QFrame::StyledPanel);
    auto* previewSectionLayout = new QVBoxLayout(previewSection);
    previewSectionLayout->setContentsMargins(12, 12, 12, 12);

    m_rosterPreviewLabel =
        new QLabel(previewSection);

    m_previewLabel =
        new QLabel(previewSection);
    m_previewLabel->setObjectName(QStringLiteral("templatePreview"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setFrameShape(QFrame::StyledPanel);
    m_previewLabel->setMinimumHeight(PreviewHeight);
    m_previewLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    m_livePreviewCheckBox =
        new QCheckBox(previewSection);
    m_livePreviewCheckBox->setObjectName(QStringLiteral("livePreviewCheckBox"));
    m_livePreviewCheckBox->setChecked(true);

    m_previewStatusLabel =
        new QLabel(previewSection);
    m_previewStatusLabel->setObjectName(QStringLiteral("previewStatusLabel"));
    m_previewStatusLabel->setVisible(false);

    previewSectionLayout->addWidget(m_rosterPreviewLabel);
    previewSectionLayout->addWidget(m_previewLabel, 1);
    previewSectionLayout->addWidget(m_livePreviewCheckBox);
    previewSectionLayout->addWidget(m_previewStatusLabel);

    templateLayout->addWidget(pageLayoutOptionsWidget);
    templateLayout->addWidget(m_extraInfoOptionsGroup);
    templateLayout->addWidget(previewSection, 1);

    auto* scopeGroupBox =
        new QGroupBox(this);
    auto* scopeLayout =
        new QVBoxLayout(scopeGroupBox);

    m_scopeGroup =
        new QButtonGroup(this);

    auto* allClassesRadio =
        new QRadioButton(scopeGroupBox);
    auto* currentClassRadio =
        new QRadioButton(scopeGroupBox);
    auto* selectedClassesRadio =
        new QRadioButton(scopeGroupBox);

    allClassesRadio->setObjectName(QStringLiteral("allClassesRadio"));
    currentClassRadio->setObjectName(QStringLiteral("currentClassRadio"));
    selectedClassesRadio->setObjectName(QStringLiteral("selectedClassesRadio"));

    m_scopeGroup->addButton(allClassesRadio, AllClassesId);
    m_scopeGroup->addButton(currentClassRadio, CurrentClassId);
    m_scopeGroup->addButton(selectedClassesRadio, SelectedClassesId);

    if (auto* button = m_scopeGroup->button(scopeId(m_defaultScope)))
    {
        button->setChecked(true);
    }

    scopeLayout->addWidget(allClassesRadio);
    scopeLayout->addWidget(currentClassRadio);
    scopeLayout->addWidget(selectedClassesRadio);

    m_classList =
        new QListWidget(scopeGroupBox);
    m_classList->setSelectionMode(QAbstractItemView::NoSelection);
    m_classList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_classList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_classList->setTextElideMode(Qt::ElideNone);
    m_classList->setWordWrap(false);
    m_classList->setMinimumWidth(0);
    m_classList->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Expanding
        );
    m_classList->setItemDelegate(
        new MarqueeItemDelegate(
            m_classList,
            m_classList
            )
        );
    scopeLayout->addWidget(m_classList);

    connect(
        m_classList,
        &QListWidget::itemChanged,
        this,
        &RosterPrintDialog::updateExtraInfoColumns
        );

    auto* buttonLayout =
        new QHBoxLayout();
    buttonLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    auto* cancelButton =
        new QPushButton(
            tr("Cancel"),
            this
            );
    auto* saveAsButton =
        new QPushButton(
            tr("Save As"),
            this
            );
    auto* printButton =
        new QPushButton(
            tr("Print"),
            this
            );
    printButton->setDefault(true);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(saveAsButton);
    buttonLayout->addWidget(printButton);

    rootLayout->addWidget(templateGroupBox, 1);
    rootLayout->addWidget(scopeGroupBox);
    rootLayout->addLayout(buttonLayout);

    connect(
        m_templateCombo,
        &QComboBox::currentIndexChanged,
        this,
        &RosterPrintDialog::updateTemplateOptionsVisibility
        );

    connect(
        m_extraInfoOrientationGroup,
        &QButtonGroup::idClicked,
        this,
        &RosterPrintDialog::updateExtraInfoSelectionLimits
        );

    connect(
        m_livePreviewCheckBox,
        &QCheckBox::toggled,
        this,
        &RosterPrintDialog::updatePreview
        );

    connect(
        m_scopeGroup,
        &QButtonGroup::idClicked,
        this,
        &RosterPrintDialog::updateClassListVisibility
        );

    connect(
        printButton,
        &QPushButton::clicked,
        this,
        &RosterPrintDialog::acceptPrint
        );

    connect(
        saveAsButton,
        &QPushButton::clicked,
        this,
        &RosterPrintDialog::chooseSavePath
        );

    connect(
        cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );

    m_baseMinimumWidth = minimumWidth();
    m_baseMinimumHeight = minimumSizeHint().height();
}

void RosterPrintDialog::resizeForExtraInfoOptions()
{
    if (!m_extraInfoOptionsGroup || !m_extraInfoOptionsGroup->isVisible())
    {
        return;
    }

    if (QLayout* rootLayout = layout())
    {
        rootLayout->invalidate();
        rootLayout->activate();

        const int requiredHeight = qMax(
            m_baseMinimumHeight,
            rootLayout->minimumSize().height()
            );
        const int maximumWidth =
            m_baseMinimumWidth * MaximumDialogWidthMultiplier
            / MaximumDialogWidthDivisor;
        int requiredWidth =
            m_baseMinimumWidth;

        if (
            m_extraColumnScrollArea
            && m_extraColumnOptionsWidget
            && m_extraColumnScrollArea->viewport()
            )
        {
            const int optionsWidth =
                m_extraColumnOptionsWidget->minimumWidth();
            const int viewportWidth =
                m_extraColumnScrollArea->viewport()->width();

            if (optionsWidth > viewportWidth)
            {
                requiredWidth =
                    width() + optionsWidth - viewportWidth;
            }
        }

        setMinimumHeight(requiredHeight);
        resize(
            qBound(
                m_baseMinimumWidth,
                qMax(width(), requiredWidth),
                maximumWidth
                ),
            qMax(height(), requiredHeight)
            );
    }
}

void RosterPrintDialog::loadClasses()
{
    if (!m_classList || !m_services || !m_services->dataService())
    {
        return;
    }

    const QList<Classroom> classes =
        m_services->dataService()->getClasses();

    for (const Classroom& classroom : classes)
    {
        const ClassInfo classInfo =
            m_services->dataService()->loadClassInfo(
                classroom.id
                );

        Teacher teacher;

        if (classInfo.teacherId > 0)
        {
            teacher =
                m_services->dataService()->getTeacher(
                    classInfo.teacherId
                    );
        }

        const QString displayName =
            SidebarNodeNaming::formatClassDisplayName(
                classInfo,
                teacher
                );

        if (classroom.id == m_currentClassId)
        {
            m_currentClassDisplayName =
                displayName;
        }

        auto* item =
            new QListWidgetItem(
                displayName,
                m_classList
                );

        item->setFlags(
            item->flags()
            | Qt::ItemIsUserCheckable
            );
        item->setCheckState(
            classroom.id == m_currentClassId
                ? Qt::Checked
                : Qt::Unchecked
            );
        item->setData(Qt::UserRole, classroom.id);
    }
}

void RosterPrintDialog::retranslateUi()
{
    setWindowTitle(tr("Print Rosters"));

    if (m_templateCombo)
    {
        if (auto* group = qobject_cast<QGroupBox*>(m_templateCombo->parentWidget()))
        {
            group->setTitle(tr("Template"));
        }

        for (int index = 0; index < m_templateCombo->count(); ++index)
        {
            const auto templateId =
                static_cast<RosterTemplatePrintService::TemplateId>(
                    m_templateCombo->itemData(index).toInt()
                    );
            m_templateCombo->setItemText(
                index,
                RosterTemplatePrintService::templateDisplayName(templateId)
                );
        }
    }

    if (m_livePreviewCheckBox)
    {
        m_livePreviewCheckBox->setText(tr("Live Preview"));
    }

    if (m_pageLayoutLabel)
    {
        m_pageLayoutLabel->setText(tr("Page Layout"));
    }

    if (m_extraInfoColumnsLabel)
    {
        m_extraInfoColumnsLabel->setText(tr("Extra Info Columns"));
    }

    if (m_extraInfoSelectionCountLabel)
    {
        int selectedCount = 0;

        for (const QCheckBox* checkBox : m_extraColumnChecks)
        {
            if (checkBox && checkBox->isChecked())
            {
                ++selectedCount;
            }
        }

        const int maxSelected =
            RosterTemplatePrintService::perClassExtraInfoMaxExtraColumns(
                selectedPerClassExtraInfoOrientation()
                );
        m_extraInfoSelectionCountLabel->setText(
            tr("%1 of %2 selected").arg(selectedCount).arg(maxSelected)
            );
    }

    if (m_rosterPreviewLabel)
    {
        m_rosterPreviewLabel->setText(tr("Roster Preview"));
    }

    if (m_extraInfoOrientationGroup)
    {
        if (auto* button = m_extraInfoOrientationGroup->button(PortraitOrientationId))
        {
            button->setText(tr("Portrait"));
        }

        if (auto* button = m_extraInfoOrientationGroup->button(LandscapeOrientationId))
        {
            button->setText(tr("Landscape"));
        }
    }

    if (m_scopeGroup)
    {
        if (auto* button = m_scopeGroup->button(AllClassesId))
        {
            button->setText(tr("All Classes"));
        }

        if (auto* button = m_scopeGroup->button(CurrentClassId))
        {
            button->setText(
                m_currentClassDisplayName.trimmed().isEmpty()
                    ? tr("Current Class")
                    : tr("Current Class (%1)").arg(m_currentClassDisplayName)
                );
        }

        if (auto* button = m_scopeGroup->button(SelectedClassesId))
        {
            button->setText(tr("Selected Classes"));
        }
    }

    if (m_classList)
    {
        if (auto* group = qobject_cast<QGroupBox*>(m_classList->parentWidget()))
        {
            group->setTitle(tr("Classes"));
        }
    }

    updatePreview();
}

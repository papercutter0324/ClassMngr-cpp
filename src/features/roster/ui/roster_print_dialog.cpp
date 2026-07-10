#include "features/roster/ui/roster_print_dialog.h"

#include "core/application_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "domain/models/class_info.h"
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

class RosterClassListMarqueeDelegate : public QStyledItemDelegate
{
public:
    explicit RosterClassListMarqueeDelegate(
        QListWidget* list,
        QObject* parent = nullptr
        )
        : QStyledItemDelegate(parent)
        , m_list(list)
    {
        m_timer.setInterval(30);

        connect(
            &m_timer,
            &QTimer::timeout,
            this,
            &RosterClassListMarqueeDelegate::advanceMarquee
            );

        if (m_list && m_list->viewport())
        {
            m_list->viewport()->setMouseTracking(true);
            m_list->viewport()->installEventFilter(this);
        }
    }

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const bool scrolling =
            m_hoveredIndex == index
            && isOverflowing(opt);

        if (!scrolling)
        {
            QStyledItemDelegate::paint(
                painter,
                option,
                index
                );
            return;
        }

        const QWidget* widget =
            opt.widget;

        QStyle* style =
            widget
                ? widget->style()
                : QApplication::style();

        QStyleOptionViewItem backgroundOpt(opt);
        backgroundOpt.text.clear();
        backgroundOpt.features &=
            ~QStyleOptionViewItem::HasDisplay;

        style->drawControl(
            QStyle::CE_ItemViewItem,
            &backgroundOpt,
            painter,
            widget
            );

        const QRect textRect =
            style->subElementRect(
                QStyle::SE_ItemViewItemText,
                &opt,
                widget
                );

        const QRect visibleTextRect =
            clippedTextRect(textRect);

        if (!visibleTextRect.isValid())
        {
            return;
        }

        const QFontMetrics metrics(opt.font);
        const int textWidth =
            metrics.horizontalAdvance(opt.text);
        const int cycleWidth =
            textWidth + ScrollGap;

        if (cycleWidth <= 0)
        {
            return;
        }

        painter->save();
        painter->setClipRect(visibleTextRect);

        const QPalette::ColorGroup colorGroup =
            (opt.state & QStyle::State_Enabled)
                ? ((opt.state & QStyle::State_Active)
                       ? QPalette::Active
                       : QPalette::Inactive)
                : QPalette::Disabled;

        const QPalette::ColorRole colorRole =
            (opt.state & QStyle::State_Selected)
                ? QPalette::HighlightedText
                : QPalette::Text;

        painter->setPen(
            opt.palette.color(
                colorGroup,
                colorRole
                )
            );
        painter->setFont(opt.font);

        const int offset =
            m_offset % cycleWidth;
        const int baseline =
            textRect.top()
            + (textRect.height() + metrics.ascent() - metrics.descent()) / 2;

        int x =
            textRect.left() - offset;

        do
        {
            painter->drawText(
                QPoint(x, baseline),
                opt.text
                );

            x += cycleWidth;
        }
        while (x < visibleTextRect.right());

        painter->restore();
    }

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override
    {
        if (
            m_list
            && watched == m_list->viewport()
            )
        {
            if (event->type() == QEvent::MouseMove)
            {
                auto* mouseEvent =
                    static_cast<QMouseEvent*>(event);

                setHoveredIndex(
                    m_list->indexAt(
                        mouseEvent->pos()
                        )
                    );
            }
            else if (event->type() == QEvent::Leave)
            {
                setHoveredIndex(
                    QModelIndex()
                    );
            }
        }

        return QStyledItemDelegate::eventFilter(
            watched,
            event
            );
    }

private:
    static constexpr int ScrollGap = 32;

    void setHoveredIndex(
        const QModelIndex& index
        )
    {
        const QModelIndex oldIndex =
            m_hoveredIndex;

        if (oldIndex == index)
        {
            return;
        }

        m_hoveredIndex =
            index;
        m_offset = 0;

        updateIndex(oldIndex);
        updateIndex(index);
        updateTimer();
    }

    void advanceMarquee()
    {
        if (
            !m_hoveredIndex.isValid()
            || !isIndexOverflowing(m_hoveredIndex)
            )
        {
            m_timer.stop();
            m_offset = 0;
            updateIndex(m_hoveredIndex);
            return;
        }

        ++m_offset;

        if (m_offset > 100000)
        {
            m_offset = 0;
        }

        updateIndex(m_hoveredIndex);
    }

    void updateTimer()
    {
        if (
            m_hoveredIndex.isValid()
            && isIndexOverflowing(m_hoveredIndex)
            )
        {
            if (!m_timer.isActive())
            {
                m_timer.start();
            }
        }
        else
        {
            m_timer.stop();
        }
    }

    void updateIndex(
        const QModelIndex& index
        ) const
    {
        if (
            !m_list
            || !m_list->viewport()
            || !index.isValid()
            )
        {
            return;
        }

        m_list->viewport()->update(
            m_list->visualRect(index)
            );
    }

    bool isIndexOverflowing(
        const QModelIndex& index
        ) const
    {
        if (
            !m_list
            || !index.isValid()
            )
        {
            return false;
        }

        QStyleOptionViewItem option;
        option.initFrom(
            m_list->viewport()
            );
        option.widget =
            m_list->viewport();
        option.rect =
            m_list->visualRect(index);

        initStyleOption(
            &option,
            index
            );

        return isOverflowing(option);
    }

    bool isOverflowing(
        const QStyleOptionViewItem& option
        ) const
    {
        if (
            !m_list
            || !m_list->viewport()
            || option.text.isEmpty()
            )
        {
            return false;
        }

        const QWidget* widget =
            option.widget;

        QStyle* style =
            widget
                ? widget->style()
                : QApplication::style();

        const QRect textRect =
            style->subElementRect(
                QStyle::SE_ItemViewItemText,
                &option,
                widget
                );

        const QRect visibleTextRect =
            clippedTextRect(textRect);

        if (!visibleTextRect.isValid())
        {
            return false;
        }

        return QFontMetrics(option.font).horizontalAdvance(option.text)
            > visibleTextRect.width();
    }

    QRect clippedTextRect(
        const QRect& textRect
        ) const
    {
        if (
            !m_list
            || !m_list->viewport()
            )
        {
            return textRect;
        }

        return textRect.intersected(
            m_list->viewport()->rect()
            );
    }

    QListWidget* m_list = nullptr;
    QTimer m_timer;
    QPersistentModelIndex m_hoveredIndex;
    int m_offset = 0;
};

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
        || !m_extraColumnList
        )
    {
        return columns;
    }

    for (int index = 0; index < m_extraColumnList->count(); ++index)
    {
        const QListWidgetItem* item =
            m_extraColumnList->item(index);

        if (item && item->checkState() == Qt::Checked)
        {
            columns.append(item->text());
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
    const bool showExtraInfoOptions =
        selectedTemplateId()
        == RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo;

    if (m_extraInfoOptionsGroup)
    {
        m_extraInfoOptionsGroup->setVisible(showExtraInfoOptions);
        m_extraInfoOptionsGroup->setEnabled(showExtraInfoOptions);
    }

    if (showExtraInfoOptions)
    {
        updateExtraInfoColumns();
    }
    else
    {
        updatePreview();
    }
}

void RosterPrintDialog::updateExtraInfoColumns()
{
    if (
        selectedTemplateId()
            != RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
        ||
        !m_extraColumnList
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

    const QSignalBlocker blocker(m_extraColumnList);
    m_extraColumnList->clear();

    for (const QString& column : columns)
    {
        auto* item =
            new QListWidgetItem(
                column,
                m_extraColumnList
                );
        item->setFlags(
            item->flags()
            | Qt::ItemIsUserCheckable
            );
        item->setCheckState(
            previouslyChecked.contains(column, Qt::CaseInsensitive)
                ? Qt::Checked
                : Qt::Unchecked
            );
    }

    updateExtraInfoSelectionLimits();
    updatePreview();
}

void RosterPrintDialog::updateExtraInfoSelectionLimits()
{
    if (!m_extraColumnList)
    {
        return;
    }

    const int maxSelected =
        RosterTemplatePrintService::perClassExtraInfoMaxExtraColumns(
            selectedPerClassExtraInfoOrientation()
            );
    int selectedCount = 0;

    {
        const QSignalBlocker blocker(m_extraColumnList);

        for (int index = 0; index < m_extraColumnList->count(); ++index)
        {
            QListWidgetItem* item =
                m_extraColumnList->item(index);

            if (!item)
            {
                continue;
            }

            if (item->checkState() == Qt::Checked)
            {
                ++selectedCount;

                if (selectedCount > maxSelected)
                {
                    item->setCheckState(Qt::Unchecked);
                    --selectedCount;
                }
            }
        }

        const bool atLimit =
            selectedCount >= maxSelected;

        for (int index = 0; index < m_extraColumnList->count(); ++index)
        {
            QListWidgetItem* item =
                m_extraColumnList->item(index);

            if (!item)
            {
                continue;
            }

            Qt::ItemFlags flags =
                item->flags()
                | Qt::ItemIsUserCheckable;

            if (
                atLimit
                && item->checkState() != Qt::Checked
                )
            {
                flags &= ~Qt::ItemIsEnabled;
            }
            else
            {
                flags |= Qt::ItemIsEnabled;
            }

            item->setFlags(flags);
        }
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

    m_extraInfoOptionsGroup =
        new QGroupBox(templateGroupBox);
    m_extraInfoOptionsGroup->setObjectName(QStringLiteral("extraInfoOptionsGroup"));
    auto* extraInfoLayout =
        new QVBoxLayout(m_extraInfoOptionsGroup);

    auto* orientationLayout =
        new QHBoxLayout();
    auto* portraitRadio =
        new QRadioButton(m_extraInfoOptionsGroup);
    auto* landscapeRadio =
        new QRadioButton(m_extraInfoOptionsGroup);
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

    m_extraColumnList =
        new QListWidget(m_extraInfoOptionsGroup);
    m_extraColumnList->setObjectName(QStringLiteral("extraColumnList"));
    m_extraColumnList->setSelectionMode(QAbstractItemView::NoSelection);
    m_extraColumnList->setMaximumHeight(120);

    extraInfoLayout->addLayout(orientationLayout);
    extraInfoLayout->addWidget(m_extraColumnList);
    m_extraInfoOptionsGroup->setVisible(false);

    m_previewLabel =
        new QLabel(templateGroupBox);
    m_previewLabel->setObjectName(QStringLiteral("templatePreview"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setFrameShape(QFrame::StyledPanel);
    m_previewLabel->setMinimumHeight(PreviewHeight);
    m_previewLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    m_livePreviewCheckBox =
        new QCheckBox(templateGroupBox);
    m_livePreviewCheckBox->setObjectName(QStringLiteral("livePreviewCheckBox"));
    m_livePreviewCheckBox->setChecked(true);

    m_previewStatusLabel =
        new QLabel(templateGroupBox);
    m_previewStatusLabel->setObjectName(QStringLiteral("previewStatusLabel"));
    m_previewStatusLabel->setVisible(false);

    templateLayout->addWidget(m_templateCombo);
    templateLayout->addWidget(m_extraInfoOptionsGroup);
    templateLayout->addWidget(m_previewLabel, 1);
    templateLayout->addWidget(m_livePreviewCheckBox);
    templateLayout->addWidget(m_previewStatusLabel);

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
        new RosterClassListMarqueeDelegate(
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

    connect(
        m_extraColumnList,
        &QListWidget::itemChanged,
        this,
        &RosterPrintDialog::updateExtraInfoSelectionLimits
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

    if (m_extraInfoOptionsGroup)
    {
        m_extraInfoOptionsGroup->setTitle(tr("Extra Info Columns"));
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

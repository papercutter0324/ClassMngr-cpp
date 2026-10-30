#include "next/ui/schedule_output_dialog.h"

#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int OptionsColumnSpacing = 32;

int requiredColumnWidth(
    const QRadioButton* first,
    const QRadioButton* second
    )
{
    return qMax(first->sizeHint().width(), second->sizeHint().width());
}

} // namespace

ScheduleOutputDialog::ScheduleOutputDialog(
    SavePathChooser chooseSavePath,
    QWidget* parent
    )
    : QDialog(parent)
    , m_chooseSavePath(std::move(chooseSavePath))
    , m_selectedKind(ScheduleOutputKind::Print)
{
    setModal(true);
    m_layout = new QVBoxLayout(this);
    auto* const layout = m_layout;

    m_styleGroup = new QGroupBox(this);
    m_styleGroup->setObjectName(QStringLiteral("nextScheduleOutputStyleGroup"));
    auto* const styleLayout = new QGridLayout(m_styleGroup);
    styleLayout->setContentsMargins(12, 10, 12, 12);
    styleLayout->setHorizontalSpacing(OptionsColumnSpacing);
    styleLayout->setVerticalSpacing(8);
    m_currentAppearance = new QRadioButton(m_styleGroup);
    m_light = new QRadioButton(m_styleGroup);
    m_dark = new QRadioButton(m_styleGroup);
    m_excel = new QRadioButton(m_styleGroup);
    m_currentAppearance->setObjectName(
        QStringLiteral("nextScheduleOutputCurrentAppearance"));
    m_light->setObjectName(QStringLiteral("nextScheduleOutputLight"));
    m_dark->setObjectName(QStringLiteral("nextScheduleOutputDark"));
    m_excel->setObjectName(QStringLiteral("nextScheduleOutputExcel"));
    m_currentAppearance->setChecked(true);
    styleLayout->addWidget(m_currentAppearance, 0, 0);
    styleLayout->addWidget(m_light, 1, 0);
    styleLayout->addWidget(m_excel, 0, 1);
    styleLayout->addWidget(m_dark, 1, 1);
    styleLayout->setColumnMinimumWidth(
        0, requiredColumnWidth(m_currentAppearance, m_light));
    styleLayout->setColumnMinimumWidth(1, requiredColumnWidth(m_excel, m_dark));
    styleLayout->setColumnStretch(0, 1);
    styleLayout->setColumnStretch(1, 1);
    layout->addWidget(m_styleGroup);

    m_orientationGroup = new QGroupBox(this);
    m_orientationGroup->setObjectName(
        QStringLiteral("nextScheduleOutputOrientationGroup"));
    auto* const orientationLayout = new QVBoxLayout(m_orientationGroup);
    orientationLayout->setContentsMargins(12, 10, 12, 12);
    orientationLayout->setSpacing(8);
    m_landscape = new QRadioButton(m_orientationGroup);
    m_portrait = new QRadioButton(m_orientationGroup);
    m_landscape->setObjectName(QStringLiteral("nextScheduleOutputLandscape"));
    m_portrait->setObjectName(QStringLiteral("nextScheduleOutputPortrait"));
    m_landscape->setChecked(true);
    orientationLayout->addWidget(m_landscape);
    orientationLayout->addWidget(m_portrait);
    layout->addWidget(m_orientationGroup);

    m_buttons = new TextFitDialogButtonBox(
        QDialogButtonBox::Cancel, this);
    layout->addWidget(m_buttons);
    m_cancelButton = m_buttons->button(QDialogButtonBox::Cancel);
    m_cancelButton->setObjectName(
        QStringLiteral("nextScheduleOutputCancel"));
    m_printButton = m_buttons->addButton(
        tr("Print"), QDialogButtonBox::ActionRole);
    m_printButton->setObjectName(QStringLiteral("nextScheduleOutputPrint"));
    m_saveButton = m_buttons->addButton(
        tr("Save As PDF…"), QDialogButtonBox::ActionRole);
    m_saveButton->setObjectName(QStringLiteral("nextScheduleOutputSaveAs"));
    m_printButton->setDefault(true);
    connect(m_printButton, &QPushButton::clicked,
        this, &ScheduleOutputDialog::acceptPrint);
    connect(m_saveButton, &QPushButton::clicked,
        this, &ScheduleOutputDialog::choosePdfPath);

    retranslateUi();
    layout->activate();
    setFixedWidth(layout->minimumSize().width());
}

void ScheduleOutputDialog::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void ScheduleOutputDialog::retranslateUi()
{
    setWindowTitle(tr("Schedule Output"));
    if (m_styleGroup)
    {
        m_styleGroup->setTitle(tr("Style"));
    }
    if (m_currentAppearance)
    {
        m_currentAppearance->setText(tr("Current Appearance"));
    }
    if (m_light)
    {
        m_light->setText(tr("Light"));
    }
    if (m_dark)
    {
        m_dark->setText(tr("Dark"));
    }
    if (m_excel)
    {
        m_excel->setText(tr("Excel"));
    }
    if (m_orientationGroup)
    {
        m_orientationGroup->setTitle(tr("Orientation"));
    }
    if (m_landscape)
    {
        m_landscape->setText(tr("Landscape"));
    }
    if (m_portrait)
    {
        m_portrait->setText(tr("Portrait"));
    }
    if (m_cancelButton)
    {
        m_cancelButton->setText(tr("Cancel"));
    }
    if (m_printButton)
    {
        m_printButton->setText(tr("Print"));
    }
    if (m_saveButton)
    {
        m_saveButton->setText(tr("Save As PDF…"));
    }
    if (m_layout)
    {
        m_layout->invalidate();
        m_layout->activate();
        setFixedWidth(m_layout->minimumSize().width());
    }
}

ScheduleOutputKind ScheduleOutputDialog::selectedKind() const noexcept
{
    return m_selectedKind;
}

SchedulePrintStyle ScheduleOutputDialog::selectedStyle() const noexcept
{
    if (m_light && m_light->isChecked())
    {
        return SchedulePrintStyle::LightTheme;
    }
    if (m_dark && m_dark->isChecked())
    {
        return SchedulePrintStyle::DarkTheme;
    }
    if (m_excel && m_excel->isChecked())
    {
        return SchedulePrintStyle::Excel;
    }
    return SchedulePrintStyle::CurrentAppearance;
}

QPageLayout::Orientation ScheduleOutputDialog::selectedOrientation() const noexcept
{
    return m_portrait && m_portrait->isChecked()
        ? QPageLayout::Portrait : QPageLayout::Landscape;
}

QString ScheduleOutputDialog::selectedPath() const
{
    return m_selectedPath;
}

void ScheduleOutputDialog::acceptPrint()
{
    m_selectedKind = ScheduleOutputKind::Print;
    m_selectedPath.clear();
    accept();
}

void ScheduleOutputDialog::choosePdfPath()
{
    if (!m_chooseSavePath)
    {
        return;
    }
    const std::optional<QString> selection = m_chooseSavePath(this);
    if (!selection || selection->trimmed().isEmpty())
    {
        return;
    }
    m_selectedPath = *selection;
    if (QFileInfo(m_selectedPath).suffix().isEmpty())
    {
        m_selectedPath += QStringLiteral(".pdf");
    }
    m_selectedKind = ScheduleOutputKind::SaveAsPdf;
    accept();
}

} // namespace ClassMngr::Next::Ui

#include "next/ui/schedule_output_dialog.h"

#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QDialogButtonBox>
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
    setWindowTitle(tr("Schedule Output"));
    setModal(true);
    m_layout = new QVBoxLayout(this);
    auto* const layout = m_layout;

    auto* const styleGroup = new QGroupBox(tr("Style"), this);
    auto* const styleLayout = new QGridLayout(styleGroup);
    styleLayout->setContentsMargins(12, 10, 12, 12);
    styleLayout->setHorizontalSpacing(OptionsColumnSpacing);
    styleLayout->setVerticalSpacing(8);
    m_currentAppearance = new QRadioButton(tr("Current Appearance"), styleGroup);
    m_light = new QRadioButton(tr("Light"), styleGroup);
    m_dark = new QRadioButton(tr("Dark"), styleGroup);
    m_excel = new QRadioButton(tr("Excel"), styleGroup);
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
    layout->addWidget(styleGroup);

    auto* const orientationGroup = new QGroupBox(tr("Orientation"), this);
    auto* const orientationLayout = new QVBoxLayout(orientationGroup);
    orientationLayout->setContentsMargins(12, 10, 12, 12);
    orientationLayout->setSpacing(8);
    m_landscape = new QRadioButton(tr("Landscape"), orientationGroup);
    m_portrait = new QRadioButton(tr("Portrait"), orientationGroup);
    m_landscape->setObjectName(QStringLiteral("nextScheduleOutputLandscape"));
    m_portrait->setObjectName(QStringLiteral("nextScheduleOutputPortrait"));
    m_landscape->setChecked(true);
    orientationLayout->addWidget(m_landscape);
    orientationLayout->addWidget(m_portrait);
    layout->addWidget(orientationGroup);

    auto* const buttons = new TextFitDialogButtonBox(
        QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(
        QStringLiteral("nextScheduleOutputCancel"));
    auto* const printButton = buttons->addButton(
        tr("Print"), QDialogButtonBox::ActionRole);
    printButton->setObjectName(QStringLiteral("nextScheduleOutputPrint"));
    auto* const saveButton = buttons->addButton(
        tr("Save As PDF…"), QDialogButtonBox::ActionRole);
    saveButton->setObjectName(QStringLiteral("nextScheduleOutputSaveAs"));
    printButton->setDefault(true);
    connect(printButton, &QPushButton::clicked,
        this, &ScheduleOutputDialog::acceptPrint);
    connect(saveButton, &QPushButton::clicked,
        this, &ScheduleOutputDialog::choosePdfPath);

    layout->activate();
    setFixedWidth(layout->minimumSize().width());
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

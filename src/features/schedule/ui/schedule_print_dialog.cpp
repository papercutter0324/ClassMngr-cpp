#include "schedule_print_dialog.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

SchedulePrintDialog::SchedulePrintDialog(
    QWidget* parent
    )
    : QDialog(parent)
{
    setWindowTitle(
        tr("Print Schedule")
        );

    auto* layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        18,
        18,
        18,
        18
        );
    layout->setSpacing(10);

    auto* styleGroup =
        new QGroupBox(
            tr("Style"),
            this
            );
    auto* styleLayout =
        new QVBoxLayout(styleGroup);
    styleLayout->setContentsMargins(
        12,
        10,
        12,
        12
        );
    styleLayout->setSpacing(8);

    m_currentAppearanceButton =
        new QRadioButton(
            tr("Current appearance"),
            styleGroup
            );
    m_lightThemeButton =
        new QRadioButton(
            tr("Light theme"),
            styleGroup
            );
    m_darkThemeButton =
        new QRadioButton(
            tr("Dark theme"),
            styleGroup
            );
    m_excelButton =
        new QRadioButton(
            tr("Excel-style"),
            styleGroup
            );

    m_currentAppearanceButton->setChecked(true);

    styleLayout->addWidget(m_currentAppearanceButton);
    styleLayout->addWidget(m_lightThemeButton);
    styleLayout->addWidget(m_darkThemeButton);
    styleLayout->addWidget(m_excelButton);
    layout->addWidget(styleGroup);

    auto* orientationGroup =
        new QGroupBox(
            tr("Orientation"),
            this
            );
    auto* orientationLayout =
        new QVBoxLayout(orientationGroup);
    orientationLayout->setContentsMargins(
        12,
        10,
        12,
        12
        );
    orientationLayout->setSpacing(8);

    m_landscapeButton =
        new QRadioButton(
            tr("Landscape"),
            orientationGroup
            );
    m_portraitButton =
        new QRadioButton(
            tr("Portrait"),
            orientationGroup
            );

    m_landscapeButton->setChecked(true);

    orientationLayout->addWidget(m_landscapeButton);
    orientationLayout->addWidget(m_portraitButton);
    layout->addWidget(orientationGroup);

    auto* buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            this
            );

    buttons->button(QDialogButtonBox::Ok)->setText(
        tr("Print")
        );

    connect(
        buttons,
        &QDialogButtonBox::accepted,
        this,
        &QDialog::accept
        );
    connect(
        buttons,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );

    layout->addWidget(buttons);
}

SchedulePrintStyle SchedulePrintDialog::selectedStyle() const
{
    if (m_lightThemeButton && m_lightThemeButton->isChecked())
    {
        return SchedulePrintStyle::LightTheme;
    }

    if (m_darkThemeButton && m_darkThemeButton->isChecked())
    {
        return SchedulePrintStyle::DarkTheme;
    }

    if (m_excelButton && m_excelButton->isChecked())
    {
        return SchedulePrintStyle::Excel;
    }

    return SchedulePrintStyle::CurrentAppearance;
}

QPageLayout::Orientation SchedulePrintDialog::selectedOrientation() const
{
    if (m_portraitButton && m_portraitButton->isChecked())
    {
        return QPageLayout::Portrait;
    }

    return QPageLayout::Landscape;
}

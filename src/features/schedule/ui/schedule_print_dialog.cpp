#include "schedule_print_dialog.h"

#include <QDialogButtonBox>
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

    m_currentAppearanceButton =
        new QRadioButton(
            tr("Current appearance"),
            this
            );
    m_lightThemeButton =
        new QRadioButton(
            tr("Light theme"),
            this
            );
    m_darkThemeButton =
        new QRadioButton(
            tr("Dark theme"),
            this
            );
    m_excelButton =
        new QRadioButton(
            tr("Excel-style"),
            this
            );

    m_currentAppearanceButton->setChecked(true);

    layout->addWidget(m_currentAppearanceButton);
    layout->addWidget(m_lightThemeButton);
    layout->addWidget(m_darkThemeButton);
    layout->addWidget(m_excelButton);

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

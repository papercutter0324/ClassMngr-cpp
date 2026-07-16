#include "schedule_print_dialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace
{
constexpr int OptionsColumnSpacing = 32;

int requiredColumnWidth(
    const QRadioButton* firstButton,
    const QRadioButton* secondButton
    )
{
    return qMax(
        firstButton->sizeHint().width(),
        secondButton->sizeHint().width()
        );
}
}

SchedulePrintDialog::SchedulePrintDialog(
    QWidget* parent
    )
    : QDialog(parent)
{
    setWindowTitle(tr("Export Schedule"));

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
        new QGridLayout(styleGroup);
    styleLayout->setContentsMargins(
        12,
        10,
        12,
        12
        );
    styleLayout->setHorizontalSpacing(OptionsColumnSpacing);
    styleLayout->setVerticalSpacing(8);

    m_currentAppearanceButton =
        new QRadioButton(
            tr("Current Theme"),
            styleGroup
            );
    m_lightThemeButton =
        new QRadioButton(
            tr("Light Theme"),
            styleGroup
            );
    m_darkThemeButton =
        new QRadioButton(
            tr("Dark Theme"),
            styleGroup
            );
    m_excelButton =
        new QRadioButton(
            tr("Excel Theme"),
            styleGroup
            );

    m_currentAppearanceButton->setObjectName(
        QStringLiteral("schedulePrintCurrentThemeButton")
        );
    m_lightThemeButton->setObjectName(
        QStringLiteral("schedulePrintLightThemeButton")
        );
    m_excelButton->setObjectName(
        QStringLiteral("schedulePrintExcelThemeButton")
        );
    m_darkThemeButton->setObjectName(
        QStringLiteral("schedulePrintDarkThemeButton")
        );

    m_currentAppearanceButton->setChecked(true);

    styleLayout->addWidget(m_currentAppearanceButton, 0, 0);
    styleLayout->addWidget(m_lightThemeButton, 1, 0);
    styleLayout->addWidget(m_excelButton, 0, 1);
    styleLayout->addWidget(m_darkThemeButton, 1, 1);
    styleLayout->setColumnMinimumWidth(
        0,
        requiredColumnWidth(
            m_currentAppearanceButton,
            m_lightThemeButton
            )
        );
    styleLayout->setColumnMinimumWidth(
        1,
        requiredColumnWidth(
            m_excelButton,
            m_darkThemeButton
            )
        );
    styleLayout->setColumnStretch(0, 1);
    styleLayout->setColumnStretch(1, 1);
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

    auto* buttonLayout = new QHBoxLayout;
    auto* cancelButton =
        new QPushButton(
            tr("Cancel"),
            this
            );
    cancelButton->setObjectName(
        QStringLiteral("schedulePrintCancelButton")
        );
    auto* saveAsButton =
        new QPushButton(
            tr("Save As"),
            this
            );
    saveAsButton->setObjectName(
        QStringLiteral("schedulePrintSaveAsButton")
        );
    auto* printButton =
        new QPushButton(
            tr("Print"),
            this
            );
    printButton->setObjectName(
        QStringLiteral("schedulePrintButton")
        );
    printButton->setDefault(true);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(saveAsButton);
    buttonLayout->addWidget(printButton);

    connect(
        cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );
    connect(
        saveAsButton,
        &QPushButton::clicked,
        this,
        &SchedulePrintDialog::chooseSavePath
        );
    connect(
        printButton,
        &QPushButton::clicked,
        this,
        &SchedulePrintDialog::acceptPrint
        );

    layout->addLayout(buttonLayout);
    layout->activate();
    setFixedWidth(layout->minimumSize().width());
}

SchedulePrintDialog::Action SchedulePrintDialog::selectedAction() const
{
    return m_selectedAction;
}

QString SchedulePrintDialog::selectedSavePath() const
{
    return m_selectedSavePath;
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

void SchedulePrintDialog::acceptPrint()
{
    m_selectedAction = Action::Print;
    m_selectedSavePath.clear();
    accept();
}

void SchedulePrintDialog::chooseSavePath()
{
    QFileDialog dialog(
        this,
        tr("Save Schedule As"),
        QString(),
        tr("PDF Documents (*.pdf)")
        );
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setDefaultSuffix(QStringLiteral("pdf"));
    dialog.selectFile(QStringLiteral("Schedule.pdf"));

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedFiles = dialog.selectedFiles();

    if (selectedFiles.isEmpty())
    {
        return;
    }

    QString savePath = selectedFiles.first();

    if (QFileInfo(savePath).suffix().isEmpty())
    {
        savePath += QStringLiteral(".pdf");
    }

    m_selectedAction = Action::SaveAs;
    m_selectedSavePath = savePath;
    accept();
}

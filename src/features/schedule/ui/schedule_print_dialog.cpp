#include "schedule_print_dialog.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include "ui/shared/dialogs/file_dialog_service.h"
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
    Action action,
    QWidget* parent
    )
    : QDialog(parent)
    , m_selectedAction(action)
{
    setWindowTitle(
        action == Action::Print
            ? tr("Print Schedule")
            : tr("Save Schedule As")
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
        new TextFitPushButton(
            tr("Cancel"),
            this
            );
    cancelButton->setObjectName(
        QStringLiteral("schedulePrintCancelButton")
        );
    auto* outputButton =
        new TextFitPushButton(
            action == Action::Print
                ? tr("Print")
                : tr("Save As..."),
            this
            );
    outputButton->setObjectName(
        action == Action::Print
            ? QStringLiteral("schedulePrintButton")
            : QStringLiteral("schedulePrintSaveAsButton")
        );
    outputButton->setDefault(true);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(outputButton);

    connect(
        cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );
    connect(
        outputButton,
        &QPushButton::clicked,
        this,
        action == Action::Print
            ? &SchedulePrintDialog::acceptPrint
            : &SchedulePrintDialog::chooseSavePath
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
    const std::optional<QString> selection =
        DialogServices::fileDialogs().saveFile(
            SaveFileRequest{
                .parent = this,
                .title = tr("Save Schedule As"),
                .purpose = FileDialogPurpose::ExportReport,
                .suggestedFileName = QStringLiteral("Schedule.pdf"),
                .nameFilters = {tr("PDF Documents (*.pdf)")},
                .defaultSuffix = QStringLiteral("pdf")
            }
            );

    if (!selection)
    {
        return;
    }

    QString savePath = *selection;

    if (QFileInfo(savePath).suffix().isEmpty())
    {
        savePath += QStringLiteral(".pdf");
    }

    m_selectedAction = Action::SaveAs;
    m_selectedSavePath = savePath;
    accept();
}

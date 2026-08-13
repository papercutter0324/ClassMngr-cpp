#include "schedule_settings_dialog.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

ScheduleSettingsDialog::ScheduleSettingsDialog(
    ScheduleService* scheduleService,
    const ScheduleSettingsValues& values,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("scheduleSettings"), parent)
    , m_scheduleService(scheduleService)
    , m_initialValues(values)
{
    setWindowTitle(tr("Schedule Settings"));
    setModal(true);
    setMinimumWidth(520);
    buildUi();
}

ScheduleSettingsValues ScheduleSettingsDialog::values() const
{
    ScheduleSettingsValues result;
    result.use24HourTime =
        m_use24HourCheck->isChecked();
    result.showEnglishNames =
        m_showEnglishNamesCheck->isChecked();
    result.showWeekends =
        m_showWeekendsCheck->isChecked();
    result.showAllIntensiveHours =
        m_showAllIntensiveHoursCheck->isChecked();
    result.testingAffectsM1 =
        m_testingAffectsM1Check->isChecked();

    return result;
}

void ScheduleSettingsDialog::buildUi()
{
    auto* layout = contentLayout();

    auto* tabs =
        new QTabWidget(this);
    tabs->setObjectName(
        QStringLiteral("scheduleSettingsTabs")
        );
    tabs->addTab(buildDisplayTab(), tr("Display"));
    tabs->addTab(buildIntensiveTab(), tr("Intensive"));
    tabs->addTab(buildTestingTab(), tr("Testing"));
    layout->addWidget(tabs);

    auto* buttons = addButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel
        );
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
}

QWidget* ScheduleSettingsDialog::buildDisplayTab()
{
    auto* page =
        new QWidget(this);
    auto* layout =
        new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_showEnglishNamesCheck =
        new QCheckBox(
            tr("Show English Names"),
            page
            );
    m_showEnglishNamesCheck->setObjectName(
        QStringLiteral("scheduleSettingsShowEnglishNames")
        );
    m_showEnglishNamesCheck->setChecked(
        m_initialValues.showEnglishNames
        );

    m_use24HourCheck =
        new QCheckBox(
            tr("Use 24-Hour Time"),
            page
            );
    m_use24HourCheck->setObjectName(
        QStringLiteral("scheduleSettingsUse24HourTime")
        );
    m_use24HourCheck->setChecked(
        m_initialValues.use24HourTime
        );

    m_showWeekendsCheck =
        new QCheckBox(
            tr("Show Weekends"),
            page
            );
    m_showWeekendsCheck->setObjectName(
        QStringLiteral("scheduleSettingsShowWeekends")
        );
    m_showWeekendsCheck->setChecked(
        m_initialValues.showWeekends
        );

    layout->addWidget(m_showEnglishNamesCheck);
    layout->addWidget(m_use24HourCheck);
    layout->addWidget(m_showWeekendsCheck);
    layout->addStretch(1);
    return page;
}

QWidget* ScheduleSettingsDialog::buildIntensiveTab()
{
    auto* page =
        new QWidget(this);
    auto* layout =
        new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_showAllIntensiveHoursCheck =
        new QCheckBox(
            tr("Show All Hours"),
            page
            );
    m_showAllIntensiveHoursCheck->setObjectName(
        QStringLiteral("scheduleSettingsShowAllIntensiveHours")
        );
    m_showAllIntensiveHoursCheck->setChecked(
        m_initialValues.showAllIntensiveHours
        );
    layout->addWidget(m_showAllIntensiveHoursCheck);

    auto* explanation =
        new QLabel(
            tr("When disabled, empty hours at the beginning and end of an intensive schedule are hidden."),
            page
            );
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    layout->addStretch(1);
    return page;
}

QWidget* ScheduleSettingsDialog::buildTestingTab()
{
    auto* page =
        new QWidget(this);
    auto* layout =
        new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* explanation =
        new QLabel(
            tr("M2 and M3 classes are always hidden in Testing mode. Oral Testing blocks and testing-class assignments are saved as one reusable weekly layout."),
            page
            );
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    m_testingAffectsM1Check =
        new QCheckBox(
            tr("Testing also affects M1"),
            page
            );
    m_testingAffectsM1Check->setObjectName(
        QStringLiteral("scheduleSettingsTestingAffectsM1")
        );
    m_testingAffectsM1Check->setChecked(
        m_initialValues.testingAffectsM1
        );
    layout->addWidget(m_testingAffectsM1Check);

    auto* clearButton =
        new TextFitPushButton(
            tr("Clear Testing Layout"),
            page
            );
    clearButton->setObjectName(
        QStringLiteral("scheduleSettingsClearTestingLayout")
        );
    layout->addWidget(
        clearButton,
        0,
        Qt::AlignLeft
        );
    layout->addStretch(1);

    connect(
        clearButton,
        &QPushButton::clicked,
        this,
        &ScheduleSettingsDialog::clearTestingLayout
        );

    return page;
}

void ScheduleSettingsDialog::clearTestingLayout()
{
    if (!m_scheduleService || !m_scheduleService->isAvailable())
    {
        DialogServices::showWarning(
            this,
            tr("Clear Testing Layout"),
            tr("No Teacher Profile is open.")
            );
        return;
    }

    const PromptChoice answer =
        DialogServices::confirm(
            this,
            tr("Clear Testing Layout?"),
            tr("This removes every Oral Testing block and testing-class assignment. Saved testing classes and their rosters are preserved."),
            tr("Clear Layout"),
            tr("Cancel"),
            true
            );

    if (answer != PromptChoice::Destructive)
    {
        return;
    }

    const Status result =
        m_scheduleService->clearTestingAssignments();

    if (!result)
    {
        DialogServices::showWarning(
            this,
            tr("Clear Testing Layout"),
            result.error()
            );
        return;
    }

    emit testingBlocksCleared();
}

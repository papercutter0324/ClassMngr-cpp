#include "calendar_settings_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"
#include "academic_calendar_provider.h"
#include "core/fontmanager.h"
#include "features/calendar/calendar_event_import_service.h"
#include "features/calendar/calendar_settings_keys.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
constexpr int DateFieldWidth = 138;
constexpr int WeekFieldWidth = 86;
constexpr int SchoolColumnSpacing = 32;
constexpr int MaximumTermWeeks = 53;
const QDate EmptyDate(1900, 1, 1);

SchoolLevel schoolLevel(int index)
{
    return index == 0
        ? SchoolLevel::Elementary
        : SchoolLevel::Middle;
}

AcademicTerm academicTerm(int index)
{
    return static_cast<AcademicTerm>(index);
}
}

CalendarSettingsDialog::CalendarSettingsDialog(
    AcademicCalendarProvider* provider,
    CalendarService* calendarService,
    SettingsService* settingsService,
    int termYear,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("calendarSettings"), parent)
    , m_provider(provider)
    , m_calendarService(calendarService)
    , m_settingsService(settingsService)
    , m_importService(
        new CalendarEventImportService(
            calendarService,
            this
            )
        )
    , m_termYear(
        qMax(termYear, AcademicCalendarSchedule::FirstTermYear)
        )
{
    buildUi();
    loadOptions();
    loadSchedules();
}

void CalendarSettingsDialog::accept()
{
    if (!m_provider)
    {
        reject();
        return;
    }

    for (const AcademicYearSchedule& schedule : m_schedules)
    {
        if (!schedule.isValid())
        {
            DialogServices::showWarning(
                this,
                tr("Invalid Academic Schedule"),
                tr("Every term must start on a Monday and last between 1 and 53 weeks.")
                );
            return;
        }
    }

    if (m_termYear > AcademicCalendarSchedule::FirstTermYear)
    {
        for (int school = 0; school < SchoolCount; ++school)
        {
            const AcademicYearSchedule previous =
                m_provider->schedule().yearSchedule(
                    schoolLevel(school),
                    m_termYear - 1
                    );
            const int days =
                previous
                    .termStart(AcademicTerm::Fall)
                    .daysTo(m_schedules[school].winterStart);

            if (
                days < 7
                || days % 7 != 0
                || days / 7 > MaximumTermWeeks
                )
            {
                DialogServices::showWarning(
                    this,
                    tr("Invalid Winter Start"),
                    tr("Winter must begin 1 to 53 complete weeks after the preceding Fall term starts.")
                    );
                return;
            }
        }
    }

    if (m_provider->hasCustomYearAfter(m_termYear))
    {
        const PromptChoice answer =
            DialogServices::confirm(
                this,
                tr("Recalculate Later Years?"),
                tr("Saving term year %1 will replace later custom schedules with predictions based on the default term durations.")
                    .arg(m_termYear),
                tr("Save"),
                tr("Cancel"),
                true
                );

        if (answer != PromptChoice::Destructive)
        {
            return;
        }
    }

    m_provider->saveYearSchedules(
        m_termYear,
        m_schedules[0],
        m_schedules[1]
        );
    saveOptions();
    QDialog::accept();
}

void CalendarSettingsDialog::restoreDefaults()
{
    if (!m_provider)
    {
        return;
    }

    for (int school = 0; school < SchoolCount; ++school)
    {
        m_schedules[school] =
            m_provider->schedule().defaultYearSchedule(
                schoolLevel(school),
                m_termYear
                );
    }

    m_linkCheck->setChecked(true);
    synchronizeLinkedTerms();
    m_dirty = true;
    refreshFields();
}

void CalendarSettingsDialog::resetCalendarEvents()
{
    if (!m_calendarService || !m_calendarService->isAvailable())
    {
        return;
    }

    const PromptChoice answer =
        DialogServices::confirm(
            this,
            tr("Reset Calendar?"),
            tr("This will permanently delete all calendar events and restore the calendar event list to defaults. This cannot be undone."),
            tr("Reset"),
            tr("Cancel"),
            true
            );

    if (answer != PromptChoice::Destructive)
    {
        return;
    }

    m_calendarService->deleteAllEvents();
    emit calendarEventsImported();

    if (m_importStatusLabel)
    {
        m_importStatusLabel->setText(
            tr("Calendar events reset to defaults.")
            );
    }
}

void CalendarSettingsDialog::linkWinterSpring(bool linked)
{
    if (m_refreshing)
    {
        return;
    }

    if (linked)
    {
        synchronizeLinkedTerms();
        m_dirty = true;
        refreshFields();
    }

    updateLinkedFieldAvailability();
}

void CalendarSettingsDialog::importCalendarEvents()
{
    if (!m_importService || m_importService->isImporting())
    {
        return;
    }

    m_importButton->setEnabled(false);
    m_importStatusLabel->setText(
        tr("Importing events...")
        );
    m_importService->importFromDefaultSource();
}

void CalendarSettingsDialog::handleImportFinished(
    int importedCount,
    int skippedCount
    )
{
    if (m_importButton)
    {
        m_importButton->setEnabled(true);
    }

    if (m_importStatusLabel)
    {
        m_importStatusLabel->setText(
            tr("Imported %1 event(s). Skipped %2 existing or ignored item(s).")
                .arg(importedCount)
                .arg(skippedCount)
            );
    }

    if (importedCount > 0)
    {
        emit calendarEventsImported();
    }
}

void CalendarSettingsDialog::handleImportFailed(
    const QString& message
    )
{
    if (m_importButton)
    {
        m_importButton->setEnabled(true);
    }

    if (m_importStatusLabel)
    {
        m_importStatusLabel->setText(
            tr("Import failed: %1").arg(message)
            );
    }
}

void CalendarSettingsDialog::buildUi()
{
    setWindowTitle(tr("Academic Calendar Settings"));
    setMinimumWidth(820);

    auto* mainLayout = contentLayout();

    auto* title =
        new QLabel(
            tr("Academic Term Schedule — %1").arg(m_termYear),
            this
            );
    title->setObjectName(QStringLiteral("pageTitle"));
    title->setAlignment(Qt::AlignCenter);
    title->setFont(
        FontManager::getUiFont(16, QFont::DemiBold)
        );
    mainLayout->addWidget(title);

    auto* tabs =
        new QTabWidget(this);
    tabs->addTab(
        buildOptionsTab(),
        tr("Options")
        );
    tabs->addTab(
        buildTermSchedulesTab(),
        tr("Term Schedules")
        );
    tabs->addTab(
        buildImportTab(),
        tr("Import")
        );
    mainLayout->addWidget(tabs);

    m_buttons = addButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel
        );

    m_buttons->button(
        QDialogButtonBox::Save
        )->setText(
            tr("Save")
            );

    m_buttons->button(
        QDialogButtonBox::Cancel
        )->setText(
            tr("Cancel")
            );

}

QWidget* CalendarSettingsDialog::buildOptionsTab()
{
    auto* page =
        new QWidget(this);
    auto* layout =
        new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    m_showAllCampusesCheck =
        new QCheckBox(
            tr("Show Events at All Campuses"),
            page
            );
    layout->addWidget(m_showAllCampusesCheck);

    m_hideStartOfTermEventsCheck =
        new QCheckBox(
            tr("Hide Start of Term Events"),
            page
            );
    layout->addWidget(m_hideStartOfTermEventsCheck);

    m_startWeekOnMondayCheck =
        new QCheckBox(
            tr("Start Calendar Weeks on Monday"),
            page
            );
    layout->addWidget(m_startWeekOnMondayCheck);

    m_resetEventsButton =
        new TextFitPushButton(
            tr("Reset Calendar to Defaults"),
            page
            );
    m_resetEventsButton->setMinimumHeight(32);
    layout->addWidget(
        m_resetEventsButton,
        0,
        Qt::AlignLeft
        );
    layout->addStretch(1);

    connect(
        m_resetEventsButton,
        &QPushButton::clicked,
        this,
        &CalendarSettingsDialog::resetCalendarEvents
        );

    return page;
}

QWidget* CalendarSettingsDialog::buildTermSchedulesTab()
{
    auto* page =
        new QWidget(this);
    auto* mainLayout =
        new QVBoxLayout(page);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    auto* fields =
        new QGridLayout;
    fields->setHorizontalSpacing(10);
    fields->setVerticalSpacing(8);

    auto* elementaryHeader =
        new QLabel(tr("Elementary"), this);
    elementaryHeader->setAlignment(Qt::AlignCenter);
    elementaryHeader->setFont(
        FontManager::getUiFont(11, QFont::DemiBold)
        );
    fields->addWidget(elementaryHeader, 0, 1, 1, 2);

    auto* middleHeader =
        new QLabel(tr("Middle School"), this);
    middleHeader->setAlignment(Qt::AlignCenter);
    middleHeader->setFont(
        FontManager::getUiFont(11, QFont::DemiBold)
        );
    fields->addWidget(middleHeader, 0, 4, 1, 2);

    auto addCenteredHeader =
        [this, fields](const QString& text, int column)
        {
            auto* label = new QLabel(text, this);
            label->setAlignment(Qt::AlignCenter);
            fields->addWidget(label, 1, column);
        };

    addCenteredHeader(tr("Term"), 0);
    addCenteredHeader(tr("Start"), 1);
    addCenteredHeader(tr("Weeks"), 2);
    addCenteredHeader(tr("Start"), 4);
    addCenteredHeader(tr("Weeks"), 5);
    fields->setColumnMinimumWidth(3, SchoolColumnSpacing);

    for (int term = 0; term < AcademicTermCount; ++term)
    {
        auto* termLabel =
            new QLabel(termName(term), this);
        termLabel->setAlignment(Qt::AlignCenter);
        fields->addWidget(termLabel, term + 2, 0);

        for (int school = 0; school < SchoolCount; ++school)
        {
            auto* dateEdit =
                new QDateEdit(this);
            dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
            dateEdit->setCalendarPopup(true);
            dateEdit->setMinimumDate(EmptyDate);
            dateEdit->setMaximumDate(QDate(2200, 12, 31));
            dateEdit->setSpecialValueText(QString());
            dateEdit->setFixedWidth(DateFieldWidth);
            m_dateEdits[school][term] = dateEdit;

            auto* weekEdit =
                new QSpinBox(this);
            weekEdit->setRange(0, MaximumTermWeeks);
            weekEdit->setSpecialValueText(QString());
            weekEdit->setFixedWidth(WeekFieldWidth);
            weekEdit->setAlignment(Qt::AlignCenter);
            m_weekEdits[school][term] = weekEdit;

            const int startColumn = school == 0 ? 1 : 4;
            fields->addWidget(dateEdit, term + 2, startColumn);
            fields->addWidget(weekEdit, term + 2, startColumn + 1);

            connect(
                dateEdit,
                &QDateEdit::dateChanged,
                this,
                [this, school, term](const QDate&)
                {
                    commitDate(school, term);
                }
                );
            connect(
                weekEdit,
                qOverload<int>(&QSpinBox::valueChanged),
                this,
                [this, school, term](int)
                {
                    commitWeeks(school, term);
                }
                );
        }
    }

    auto* centeredFields =
        new QHBoxLayout;
    centeredFields->addStretch(1);
    centeredFields->addLayout(fields);
    centeredFields->addStretch(1);
    mainLayout->addLayout(centeredFields);

    m_linkCheck =
        new QCheckBox(
            tr("Elementary and Middle School follow the same Winter and Spring term schedules."),
            page
            );
    mainLayout->addWidget(
        m_linkCheck,
        0,
        Qt::AlignHCenter
        );

    m_restoreButton =
        new TextFitPushButton(
            tr("Restore Term Defaults"),
            page
            );
    mainLayout->addWidget(
        m_restoreButton,
        0,
        Qt::AlignHCenter
        );
    mainLayout->addStretch(1);

    connect(
        m_linkCheck,
        &QCheckBox::toggled,
        this,
        &CalendarSettingsDialog::linkWinterSpring
        );
    connect(
        m_restoreButton,
        &QPushButton::clicked,
        this,
        &CalendarSettingsDialog::restoreDefaults
        );

    return page;
}

QWidget* CalendarSettingsDialog::buildImportTab()
{
    auto* page =
        new QWidget(this);
    auto* mainLayout =
        new QVBoxLayout(page);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    auto* importHeader =
        new QLabel(
            tr("Import Events"),
            page
            );
    importHeader->setAlignment(Qt::AlignCenter);
    importHeader->setFont(
        FontManager::getUiFont(12, QFont::DemiBold)
        );
    mainLayout->addWidget(importHeader);

    auto* importLayout =
        new QHBoxLayout;
    importLayout->setSpacing(8);

    m_importUrlEdit =
        new QLineEdit(
            CalendarEventImportService::defaultImportUrl(),
            page
            );
    m_importUrlEdit->setReadOnly(true);
    m_importUrlEdit->setMinimumWidth(420);

    m_importButton =
        new TextFitPushButton(
            tr("Import Events"),
            page
            );

    importLayout->addWidget(m_importUrlEdit, 1);
    importLayout->addWidget(m_importButton);
    mainLayout->addLayout(importLayout);

    m_importStatusLabel =
        new QLabel(page);
    m_importStatusLabel->setObjectName(QStringLiteral("sectionSubtitle"));
    m_importStatusLabel->setWordWrap(true);
    mainLayout->addWidget(m_importStatusLabel);

    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &CalendarSettingsDialog::importCalendarEvents
        );
    connect(
        m_importService,
        &CalendarEventImportService::importFinished,
        this,
        &CalendarSettingsDialog::handleImportFinished
        );
    connect(
        m_importService,
        &CalendarEventImportService::importFailed,
        this,
        &CalendarSettingsDialog::handleImportFailed
        );

    mainLayout->addStretch(1);

    return page;
}

void CalendarSettingsDialog::loadSchedules()
{
    if (!m_provider)
    {
        return;
    }

    m_schedules[0] =
        m_provider->schedule().yearSchedule(
            SchoolLevel::Elementary,
            m_termYear
            );
    m_schedules[1] =
        m_provider->schedule().yearSchedule(
            SchoolLevel::Middle,
            m_termYear
            );

    const bool matching =
        m_schedules[0].winterStart == m_schedules[1].winterStart
        && m_schedules[0].weeks[0] == m_schedules[1].weeks[0]
        && m_schedules[0].weeks[1] == m_schedules[1].weeks[1];

    m_refreshing = true;
    m_linkCheck->setChecked(matching);
    m_refreshing = false;
    m_dirty = false;
    refreshFields();
}

void CalendarSettingsDialog::loadOptions()
{
    if (m_showAllCampusesCheck)
    {
        m_showAllCampusesCheck->setChecked(
            m_settingsService && m_settingsService->isAvailable()
                ? m_settingsService
                    ->load(
                        CalendarSettingsKeys::ShowEventsAtAllCampuses,
                        false
                        )
                    .toBool()
                : false
            );
    }

    if (m_startWeekOnMondayCheck && m_provider)
    {
        m_startWeekOnMondayCheck->setChecked(
            m_provider->firstDayOfWeek() == 1
            );
    }

    if (m_hideStartOfTermEventsCheck)
    {
        m_hideStartOfTermEventsCheck->setChecked(
            m_settingsService && m_settingsService->isAvailable()
                ? m_settingsService
                    ->load(
                        CalendarSettingsKeys::HideStartOfTermEvents,
                        false
                        )
                    .toBool()
                : false
            );
    }
}

void CalendarSettingsDialog::saveOptions()
{
    if (
        m_settingsService
        && m_settingsService->isAvailable()
        && m_showAllCampusesCheck
        )
    {
        m_settingsService->save(
            CalendarSettingsKeys::ShowEventsAtAllCampuses,
            m_showAllCampusesCheck->isChecked()
            );
    }

    if (
        m_settingsService
        && m_settingsService->isAvailable()
        && m_hideStartOfTermEventsCheck
        )
    {
        m_settingsService->save(
            CalendarSettingsKeys::HideStartOfTermEvents,
            m_hideStartOfTermEventsCheck->isChecked()
            );
    }

    if (m_provider && m_startWeekOnMondayCheck)
    {
        m_provider->setFirstDayOfWeek(
            m_startWeekOnMondayCheck->isChecked() ? 1 : 0
            );
    }
}

void CalendarSettingsDialog::refreshFields()
{
    m_refreshing = true;

    for (int school = 0; school < SchoolCount; ++school)
    {
        for (int term = 0; term < AcademicTermCount; ++term)
        {
            const QSignalBlocker dateBlocker(m_dateEdits[school][term]);
            const QSignalBlocker weekBlocker(m_weekEdits[school][term]);

            m_dateEdits[school][term]->setDate(
                m_schedules[school].termStart(academicTerm(term))
                );
            m_weekEdits[school][term]->setValue(
                m_schedules[school].weeks[term]
                );
        }
    }

    m_refreshing = false;
    updateLinkedFieldAvailability();
}

void CalendarSettingsDialog::commitDate(
    int schoolIndex,
    int termIndex
    )
{
    if (m_refreshing)
    {
        return;
    }

    const QDate edited =
        m_dateEdits[schoolIndex][termIndex]->date();

    if (edited == EmptyDate)
    {
        clearDate(schoolIndex, termIndex);
        return;
    }

    if (edited.dayOfWeek() != Qt::Monday)
    {
        DialogServices::showWarning(
            this,
            tr("Monday Required"),
            tr("Academic terms must start on a Monday.")
            );
        refreshFields();
        return;
    }

    AcademicYearSchedule& schedule =
        m_schedules[schoolIndex];

    if (termIndex == 0)
    {
        schedule.winterStart = edited;
    }
    else
    {
        const QDate previousStart =
            schedule.termStart(academicTerm(termIndex - 1));
        const int days = previousStart.daysTo(edited);
        const int weeks = days / 7;

        if (
            days <= 0
            || days % 7 != 0
            || weeks > MaximumTermWeeks
            )
        {
            DialogServices::showWarning(
                this,
                tr("Invalid Term Start"),
                tr("The start must be 1 to 53 complete weeks after the preceding term starts.")
                );
            refreshFields();
            return;
        }

        schedule.weeks[termIndex - 1] = weeks;
    }

    if (m_linkCheck->isChecked() && schoolIndex == 0 && termIndex <= 2)
    {
        synchronizeLinkedTerms();
    }

    m_dirty = true;
    refreshFields();
}

void CalendarSettingsDialog::clearDate(
    int schoolIndex,
    int termIndex
    )
{
    if (m_refreshing || !m_provider)
    {
        return;
    }

    AcademicYearSchedule& schedule =
        m_schedules[schoolIndex];

    if (termIndex == 0)
    {
        schedule.winterStart =
            m_provider->schedule()
                .defaultYearSchedule(
                    schoolLevel(schoolIndex),
                    m_termYear
                    )
                .winterStart;
    }

    if (m_linkCheck->isChecked() && schoolIndex == 0 && termIndex <= 2)
    {
        synchronizeLinkedTerms();
    }

    m_dirty = true;
    refreshFields();
}

void CalendarSettingsDialog::commitWeeks(
    int schoolIndex,
    int termIndex
    )
{
    if (m_refreshing)
    {
        return;
    }

    const int value =
        m_weekEdits[schoolIndex][termIndex]->value();

    if (value == 0)
    {
        refreshFields();
        return;
    }

    m_schedules[schoolIndex].weeks[termIndex] = value;

    if (
        m_linkCheck->isChecked()
        && schoolIndex == 0
        && termIndex <= 1
        )
    {
        synchronizeLinkedTerms();
    }

    m_dirty = true;
    refreshFields();
}

void CalendarSettingsDialog::synchronizeLinkedTerms()
{
    m_schedules[1].winterStart =
        m_schedules[0].winterStart;
    m_schedules[1].weeks[0] =
        m_schedules[0].weeks[0];
    m_schedules[1].weeks[1] =
        m_schedules[0].weeks[1];
}

void CalendarSettingsDialog::updateLinkedFieldAvailability()
{
    const bool linked =
        m_linkCheck && m_linkCheck->isChecked();

    for (int term = 0; term <= 1; ++term)
    {
        m_weekEdits[1][term]->setEnabled(!linked);
    }

    for (int term = 0; term <= 2; ++term)
    {
        m_dateEdits[1][term]->setEnabled(!linked);
    }
}

QString CalendarSettingsDialog::termName(int termIndex) const
{
    switch (academicTerm(termIndex))
    {
    case AcademicTerm::Winter:
        return tr("Winter");

    case AcademicTerm::Spring:
        return tr("Spring");

    case AcademicTerm::Summer:
        return tr("Summer");

    case AcademicTerm::Fall:
        return tr("Fall");
    }

    return {};
}

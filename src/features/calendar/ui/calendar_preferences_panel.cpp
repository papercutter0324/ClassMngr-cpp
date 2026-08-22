#include "calendar_preferences_panel.h"

#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"
#include "academic_calendar_provider.h"
#include "core/fontmanager.h"
#include "features/calendar/calendar_event_import_service.h"
#include "features/calendar/calendar_settings_keys.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
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

CalendarPreferencesPanel::CalendarPreferencesPanel(
    AcademicCalendarProvider* provider,
    CalendarService* calendarService,
    SettingsService* settingsService,
    QWidget* parent
    )
    : QWidget(parent)
    , m_provider(provider)
    , m_calendarService(calendarService)
    , m_settingsService(settingsService)
    , m_importService(new CalendarEventImportService(calendarService, this))
{
    setObjectName(QStringLiteral("calendarPreferencesPanel"));

    if (m_provider)
    {
        m_termYear = qMax(
            m_provider->termYearForDate(QDate::currentDate()),
            AcademicCalendarSchedule::FirstTermYear
            );
    }

    buildUi();
    loadOptions();
    loadSchedules();
}

void CalendarPreferencesPanel::saveTermSchedules()
{
    if (!m_provider)
    {
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

            if (days < 7 || days % 7 != 0 || days / 7 > MaximumTermWeeks)
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
    m_dirty = false;
    emit calendarPreferencesChanged(false);
}

void CalendarPreferencesPanel::restoreDefaults()
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

void CalendarPreferencesPanel::resetCalendarEvents()
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

    const Status deleted = m_calendarService->deleteAllEvents();
    if (!deleted)
    {
        DialogServices::showWarning(this, tr("Reset Calendar"), deleted.error());
        return;
    }

    if (m_importStatusLabel)
    {
        m_importStatusLabel->setText(tr("Calendar events reset to defaults."));
    }

    emit calendarPreferencesChanged(true);
}

void CalendarPreferencesPanel::linkWinterSpring(bool linked)
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

void CalendarPreferencesPanel::importCalendarEvents()
{
    if (!m_importService || m_importService->isImporting())
    {
        return;
    }

    m_importButton->setEnabled(false);
    m_importStatusLabel->setText(tr("Importing events..."));
    m_importService->importFromDefaultSource();
}

void CalendarPreferencesPanel::handleImportFinished(
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
        emit calendarPreferencesChanged(true);
    }
}

void CalendarPreferencesPanel::handleImportFailed(const QString& message)
{
    if (m_importButton)
    {
        m_importButton->setEnabled(true);
    }

    if (m_importStatusLabel)
    {
        m_importStatusLabel->setText(tr("Import failed: %1").arg(message));
    }
}

void CalendarPreferencesPanel::setTermYear(int termYear)
{
    if (m_refreshing || termYear == m_termYear)
    {
        return;
    }

    m_termYear = qMax(termYear, AcademicCalendarSchedule::FirstTermYear);
    loadSchedules();
}

void CalendarPreferencesPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(24);
    layout->setAlignment(Qt::AlignTop);
    layout->addWidget(buildOptionsSection());
    layout->addWidget(buildTermSchedulesSection());
    layout->addWidget(buildImportSection());
}

QWidget* CalendarPreferencesPanel::buildOptionsSection()
{
    auto* section = new QGroupBox(tr("Options"), this);
    auto* layout = new QVBoxLayout(section);
    layout->setSpacing(12);

    m_showAllCampusesCheck =
        new QCheckBox(tr("Show Events at All Campuses"), section);
    m_showAllCampusesCheck->setObjectName(
        QStringLiteral("preferencesCalendarShowAllCampuses")
        );
    layout->addWidget(m_showAllCampusesCheck);

    m_hideStartOfTermEventsCheck =
        new QCheckBox(tr("Hide Start of Term Events"), section);
    m_hideStartOfTermEventsCheck->setObjectName(
        QStringLiteral("preferencesCalendarHideStartOfTermEvents")
        );
    layout->addWidget(m_hideStartOfTermEventsCheck);

    m_startWeekOnMondayCheck =
        new QCheckBox(tr("Start Calendar Weeks on Monday"), section);
    m_startWeekOnMondayCheck->setObjectName(
        QStringLiteral("preferencesCalendarStartWeekOnMonday")
        );
    layout->addWidget(m_startWeekOnMondayCheck);

    m_resetEventsButton =
        new TextFitPushButton(tr("Reset Calendar to Defaults"), section);
    m_resetEventsButton->setObjectName(
        QStringLiteral("preferencesCalendarResetEvents")
        );
    m_resetEventsButton->setMinimumHeight(32);
    layout->addWidget(m_resetEventsButton, 0, Qt::AlignLeft);

    const auto save = [this]()
    {
        if (m_refreshing)
        {
            return;
        }

        saveOptions();
        emit calendarPreferencesChanged(false);
    };
    connect(
        m_showAllCampusesCheck,
        &QCheckBox::toggled,
        this,
        save
        );
    connect(
        m_hideStartOfTermEventsCheck,
        &QCheckBox::toggled,
        this,
        save
        );
    connect(
        m_startWeekOnMondayCheck,
        &QCheckBox::toggled,
        this,
        save
        );
    connect(
        m_resetEventsButton,
        &QPushButton::clicked,
        this,
        &CalendarPreferencesPanel::resetCalendarEvents
        );

    return section;
}

QWidget* CalendarPreferencesPanel::buildTermSchedulesSection()
{
    auto* section = new QGroupBox(tr("Term Schedules"), this);
    auto* mainLayout = new QVBoxLayout(section);
    mainLayout->setSpacing(14);

    auto* yearForm = new QFormLayout;
    yearForm->setLabelAlignment(Qt::AlignLeft);
    m_termYearSpin = new QSpinBox(section);
    m_termYearSpin->setObjectName(QStringLiteral("preferencesCalendarTermYear"));
    m_termYearSpin->setRange(AcademicCalendarSchedule::FirstTermYear, 2200);
    m_termYearSpin->setValue(m_termYear);
    yearForm->addRow(tr("Academic Year:"), m_termYearSpin);
    mainLayout->addLayout(yearForm);

    auto* fields = new QGridLayout;
    fields->setHorizontalSpacing(10);
    fields->setVerticalSpacing(8);

    auto* elementaryHeader = new QLabel(tr("Elementary"), section);
    elementaryHeader->setAlignment(Qt::AlignCenter);
    elementaryHeader->setFont(FontManager::getUiFont(11, QFont::DemiBold));
    fields->addWidget(elementaryHeader, 0, 1, 1, 2);

    auto* middleHeader = new QLabel(tr("Middle School"), section);
    middleHeader->setAlignment(Qt::AlignCenter);
    middleHeader->setFont(FontManager::getUiFont(11, QFont::DemiBold));
    fields->addWidget(middleHeader, 0, 4, 1, 2);

    const auto addCenteredHeader = [section, fields](
        const QString& text,
        int column
        )
    {
        auto* label = new QLabel(text, section);
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
        auto* termLabel = new QLabel(termName(term), section);
        termLabel->setAlignment(Qt::AlignCenter);
        fields->addWidget(termLabel, term + 2, 0);

        for (int school = 0; school < SchoolCount; ++school)
        {
            auto* dateEdit = new QDateEdit(section);
            dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
            dateEdit->setCalendarPopup(true);
            dateEdit->setMinimumDate(EmptyDate);
            dateEdit->setMaximumDate(QDate(2200, 12, 31));
            dateEdit->setSpecialValueText(QString());
            dateEdit->setFixedWidth(DateFieldWidth);
            m_dateEdits[school][term] = dateEdit;

            auto* weekEdit = new QSpinBox(section);
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

    auto* centeredFields = new QHBoxLayout;
    centeredFields->addStretch(1);
    centeredFields->addLayout(fields);
    centeredFields->addStretch(1);
    mainLayout->addLayout(centeredFields);

    m_linkCheck = new QCheckBox(
        tr("Elementary and Middle School follow the same Winter and Spring term schedules."),
        section
        );
    m_linkCheck->setObjectName(QStringLiteral("preferencesCalendarLinkWinterSpring"));
    mainLayout->addWidget(m_linkCheck, 0, Qt::AlignHCenter);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    m_restoreButton = new TextFitPushButton(tr("Restore Term Defaults"), section);
    m_restoreButton->setObjectName(
        QStringLiteral("preferencesCalendarRestoreDefaults")
        );
    buttons->addWidget(m_restoreButton);
    m_saveSchedulesButton = new TextFitPushButton(tr("Save Term Schedule"), section);
    m_saveSchedulesButton->setObjectName(
        QStringLiteral("preferencesCalendarSaveTermSchedule")
        );
    buttons->addWidget(m_saveSchedulesButton);
    buttons->addStretch(1);
    mainLayout->addLayout(buttons);

    connect(
        m_termYearSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &CalendarPreferencesPanel::setTermYear
        );
    connect(
        m_linkCheck,
        &QCheckBox::toggled,
        this,
        &CalendarPreferencesPanel::linkWinterSpring
        );
    connect(
        m_restoreButton,
        &QPushButton::clicked,
        this,
        &CalendarPreferencesPanel::restoreDefaults
        );
    connect(
        m_saveSchedulesButton,
        &QPushButton::clicked,
        this,
        &CalendarPreferencesPanel::saveTermSchedules
        );

    return section;
}

QWidget* CalendarPreferencesPanel::buildImportSection()
{
    auto* section = new QGroupBox(tr("Import"), this);
    auto* mainLayout = new QVBoxLayout(section);
    mainLayout->setSpacing(12);

    auto* importLayout = new QHBoxLayout;
    importLayout->setSpacing(8);

    m_importUrlEdit = new QLineEdit(
        CalendarEventImportService::defaultImportUrl(),
        section
        );
    m_importUrlEdit->setReadOnly(true);
    m_importUrlEdit->setMinimumWidth(420);

    m_importButton = new TextFitPushButton(tr("Import Events"), section);
    m_importButton->setObjectName(QStringLiteral("preferencesCalendarImportEvents"));

    importLayout->addWidget(m_importUrlEdit, 1);
    importLayout->addWidget(m_importButton);
    mainLayout->addLayout(importLayout);

    m_importStatusLabel = new QLabel(section);
    m_importStatusLabel->setObjectName(QStringLiteral("sectionSubtitle"));
    m_importStatusLabel->setWordWrap(true);
    mainLayout->addWidget(m_importStatusLabel);

    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &CalendarPreferencesPanel::importCalendarEvents
        );
    connect(
        m_importService,
        &CalendarEventImportService::importFinished,
        this,
        &CalendarPreferencesPanel::handleImportFinished
        );
    connect(
        m_importService,
        &CalendarEventImportService::importFailed,
        this,
        &CalendarPreferencesPanel::handleImportFailed
        );

    return section;
}

void CalendarPreferencesPanel::loadSchedules()
{
    if (!m_provider)
    {
        return;
    }

    m_schedules[0] = m_provider->schedule().yearSchedule(
        SchoolLevel::Elementary,
        m_termYear
        );
    m_schedules[1] = m_provider->schedule().yearSchedule(
        SchoolLevel::Middle,
        m_termYear
        );

    const bool matching =
        m_schedules[0].winterStart == m_schedules[1].winterStart
        && m_schedules[0].weeks[0] == m_schedules[1].weeks[0]
        && m_schedules[0].weeks[1] == m_schedules[1].weeks[1];

    m_refreshing = true;
    if (m_termYearSpin)
    {
        m_termYearSpin->setValue(m_termYear);
    }
    m_linkCheck->setChecked(matching);
    m_refreshing = false;
    m_dirty = false;
    refreshFields();
}

void CalendarPreferencesPanel::loadOptions()
{
    m_refreshing = true;

    if (m_showAllCampusesCheck)
    {
        m_showAllCampusesCheck->setChecked(
            m_settingsService && m_settingsService->isAvailable()
                ? m_settingsService->loadOrDefault(
                    CalendarSettingsKeys::ShowEventsAtAllCampuses,
                    false
                    ).toBool()
                : false
            );
    }

    if (m_startWeekOnMondayCheck && m_provider)
    {
        m_startWeekOnMondayCheck->setChecked(m_provider->firstDayOfWeek() == 1);
    }

    if (m_hideStartOfTermEventsCheck)
    {
        m_hideStartOfTermEventsCheck->setChecked(
            m_settingsService && m_settingsService->isAvailable()
                ? m_settingsService->loadOrDefault(
                    CalendarSettingsKeys::HideStartOfTermEvents,
                    false
                    ).toBool()
                : false
            );
    }

    m_refreshing = false;
}

void CalendarPreferencesPanel::saveOptions()
{
    if (m_settingsService && m_settingsService->isAvailable())
    {
        m_settingsService->save(
            CalendarSettingsKeys::ShowEventsAtAllCampuses,
            m_showAllCampusesCheck->isChecked()
            );
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

void CalendarPreferencesPanel::refreshFields()
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
            m_weekEdits[school][term]->setValue(m_schedules[school].weeks[term]);
        }
    }

    m_refreshing = false;
    updateLinkedFieldAvailability();
}

void CalendarPreferencesPanel::commitDate(int schoolIndex, int termIndex)
{
    if (m_refreshing)
    {
        return;
    }

    const QDate edited = m_dateEdits[schoolIndex][termIndex]->date();
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

    AcademicYearSchedule& schedule = m_schedules[schoolIndex];
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

        if (days <= 0 || days % 7 != 0 || weeks > MaximumTermWeeks)
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

void CalendarPreferencesPanel::clearDate(int schoolIndex, int termIndex)
{
    if (m_refreshing || !m_provider)
    {
        return;
    }

    AcademicYearSchedule& schedule = m_schedules[schoolIndex];
    if (termIndex == 0)
    {
        schedule.winterStart = m_provider->schedule().defaultYearSchedule(
            schoolLevel(schoolIndex),
            m_termYear
            ).winterStart;
    }

    if (m_linkCheck->isChecked() && schoolIndex == 0 && termIndex <= 2)
    {
        synchronizeLinkedTerms();
    }

    m_dirty = true;
    refreshFields();
}

void CalendarPreferencesPanel::commitWeeks(int schoolIndex, int termIndex)
{
    if (m_refreshing)
    {
        return;
    }

    const int value = m_weekEdits[schoolIndex][termIndex]->value();
    if (value == 0)
    {
        refreshFields();
        return;
    }

    m_schedules[schoolIndex].weeks[termIndex] = value;
    if (m_linkCheck->isChecked() && schoolIndex == 0 && termIndex <= 1)
    {
        synchronizeLinkedTerms();
    }

    m_dirty = true;
    refreshFields();
}

void CalendarPreferencesPanel::synchronizeLinkedTerms()
{
    m_schedules[1].winterStart = m_schedules[0].winterStart;
    m_schedules[1].weeks[0] = m_schedules[0].weeks[0];
    m_schedules[1].weeks[1] = m_schedules[0].weeks[1];
}

void CalendarPreferencesPanel::updateLinkedFieldAvailability()
{
    const bool linked = m_linkCheck && m_linkCheck->isChecked();

    for (int term = 0; term <= 1; ++term)
    {
        m_weekEdits[1][term]->setEnabled(!linked);
    }

    for (int term = 0; term <= 2; ++term)
    {
        m_dateEdits[1][term]->setEnabled(!linked);
    }
}

QString CalendarPreferencesPanel::termName(int termIndex) const
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

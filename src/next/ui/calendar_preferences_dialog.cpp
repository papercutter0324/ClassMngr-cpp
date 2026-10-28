#include "next/ui/calendar_preferences_dialog.h"

#include "features/calendar/ui/academic_calendar_provider.h"
#include "next/application/academic_year_schedule_policy.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

QString preferencesDialogText(const char* source)
{
    return QCoreApplication::translate("CalendarPreferencesDialog", source);
}

constexpr std::array<Domain::CalendarEventType, 6> EventTypes{
    Domain::CalendarEventType::Vacation,
    Domain::CalendarEventType::Holiday,
    Domain::CalendarEventType::Workshop,
    Domain::CalendarEventType::CM,
    Domain::CalendarEventType::Meeting,
    Domain::CalendarEventType::Other
};

QString eventTypeLabel(const int index)
{
    switch (EventTypes.at(static_cast<std::size_t>(index)))
    {
    case Domain::CalendarEventType::Vacation: return QCoreApplication::translate("CalendarEventDialog", "Vacation");
    case Domain::CalendarEventType::Holiday: return QCoreApplication::translate("CalendarEventDialog", "Holiday");
    case Domain::CalendarEventType::Workshop: return QCoreApplication::translate("CalendarEventDialog", "Workshop");
    case Domain::CalendarEventType::CM: return QCoreApplication::translate("CalendarEventDialog", "CM");
    case Domain::CalendarEventType::Meeting: return QCoreApplication::translate("CalendarEventDialog", "Meeting");
    case Domain::CalendarEventType::Other: return QCoreApplication::translate("CalendarEventDialog", "Other");
    }
    return QCoreApplication::translate("CalendarEventDialog", "Other");
}

QString defaultEventColor(const int index)
{
    static const std::array<QString, 6> colors{
        QStringLiteral("#4b6f91"),
        QStringLiteral("#7a5f9e"),
        QStringLiteral("#5f7f52"),
        QStringLiteral("#9a6b3f"),
        QStringLiteral("#8a4f5d"),
        QStringLiteral("#66727a")
    };
    return colors.at(static_cast<std::size_t>(index));
}

QString utf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

SchoolLevel schoolLevel(const int index)
{
    return index == 0 ? SchoolLevel::Elementary : SchoolLevel::Middle;
}

bool sameSchedule(
    const AcademicYearSchedule& left,
    const AcademicYearSchedule& right
    )
{
    return left.termYear == right.termYear
        && left.winterStart == right.winterStart
        && left.weeks == right.weeks;
}

} // namespace

CalendarPreferencesDialog::CalendarPreferencesDialog(
    QString databasePath,
    AcademicCalendarProvider* academicProvider,
    CalendarPreferencesActions actions,
    QWidget* parent
    )
    : QDialog(parent),
      m_databasePath(std::move(databasePath)),
      m_academicProvider(academicProvider),
      m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("nextCalendarPreferencesDialog"));
    setWindowTitle(preferencesDialogText("Calendar Preferences"));
    setModal(true);
    resize(720, 720);

    auto* outer = new QVBoxLayout(this);
    auto* options = new QGroupBox(preferencesDialogText("Options"), this);
    auto* optionsLayout = new QFormLayout(options);
    m_showAllCampuses = new QCheckBox(preferencesDialogText("Show Events at All Campuses"), options);
    m_showAllCampuses->setObjectName(QStringLiteral("calendarShowAllCampuses"));
    m_hideStartOfTerm = new QCheckBox(preferencesDialogText("Hide Start of Term Events"), options);
    m_hideStartOfTerm->setObjectName(QStringLiteral("calendarHideStartOfTerm"));
    optionsLayout->addRow(m_showAllCampuses);
    optionsLayout->addRow(m_hideStartOfTerm);

    m_firstDay = new QComboBox(options);
    m_firstDay->setObjectName(QStringLiteral("calendarFirstDayOfWeek"));
    const std::array<QString, 7> dayLabels{
        preferencesDialogText("Sunday"), preferencesDialogText("Monday"), preferencesDialogText("Tuesday"), preferencesDialogText("Wednesday"),
        preferencesDialogText("Thursday"), preferencesDialogText("Friday"), preferencesDialogText("Saturday")
    };
    for (int day = 0; day < static_cast<int>(dayLabels.size()); ++day)
    {
        m_firstDay->addItem(dayLabels.at(static_cast<std::size_t>(day)), day);
    }
    optionsLayout->addRow(preferencesDialogText("First day of week"), m_firstDay);
    outer->addWidget(options);

    auto* colors = new QGroupBox(preferencesDialogText("Event Colors"), this);
    auto* colorsLayout = new QGridLayout(colors);
    for (int type = 0; type < static_cast<int>(EventTypes.size()); ++type)
    {
        colorsLayout->addWidget(new QLabel(eventTypeLabel(type), colors), type, 0);
        m_colorButtons[type] = new QPushButton(colors);
        m_colorButtons[type]->setObjectName(
            QStringLiteral("calendarEventColor%1").arg(type));
        colorsLayout->addWidget(m_colorButtons[type], type, 1);
        connect(m_colorButtons[type], &QPushButton::clicked, this,
            [this, type] { chooseColor(type); });
        m_colors[static_cast<std::size_t>(type)] = defaultEventColor(type).toStdString();
    }
    outer->addWidget(colors);

    auto* schedules = new QGroupBox(preferencesDialogText("Academic Term Schedule"), this);
    auto* scheduleLayout = new QVBoxLayout(schedules);
    auto* yearRow = new QHBoxLayout;
    yearRow->addWidget(new QLabel(preferencesDialogText("Academic year"), schedules));
    m_termYear = new QSpinBox(schedules);
    m_termYear->setObjectName(QStringLiteral("calendarTermYear"));
    m_termYear->setRange(AcademicCalendarSchedule::FirstTermYear, 2200);
    const int currentTermYear = m_academicProvider
        ? std::max(m_academicProvider->termYearForDate(QDate::currentDate()),
                   AcademicCalendarSchedule::FirstTermYear)
        : AcademicCalendarSchedule::FirstTermYear;
    m_termYear->setValue(currentTermYear);
    yearRow->addWidget(m_termYear);
    yearRow->addStretch(1);
    m_restoreSchedule = new QPushButton(preferencesDialogText("Restore Defaults"), schedules);
    m_restoreSchedule->setObjectName(QStringLiteral("calendarTermRestoreDefaults"));
    yearRow->addWidget(m_restoreSchedule);
    scheduleLayout->addLayout(yearRow);

    auto* scheduleGrid = new QGridLayout;
    const std::array<QString, 4> termLabels{
        preferencesDialogText("Winter"), preferencesDialogText("Spring"), preferencesDialogText("Summer"), preferencesDialogText("Fall")
    };
    scheduleGrid->addWidget(new QLabel(preferencesDialogText("School level"), schedules), 0, 0);
    scheduleGrid->addWidget(new QLabel(preferencesDialogText("Winter start"), schedules), 0, 1);
    for (int term = 0; term < AcademicTermCount; ++term)
    {
        scheduleGrid->addWidget(
            new QLabel(preferencesDialogText("%1 weeks").arg(termLabels.at(
                static_cast<std::size_t>(term))), schedules),
            0,
            term + 2);
    }
    const std::array<QString, 2> schoolLabels{preferencesDialogText("Elementary"), preferencesDialogText("Middle School")};
    for (int school = 0; school < 2; ++school)
    {
        const int row = school + 1;
        scheduleGrid->addWidget(
            new QLabel(schoolLabels.at(static_cast<std::size_t>(school)), schedules),
            row, 0);
        m_winterStart[school] = new QDateEdit(schedules);
        m_winterStart[school]->setCalendarPopup(true);
        m_winterStart[school]->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        m_winterStart[school]->setObjectName(
            QStringLiteral("calendarWinterStart%1").arg(school));
        scheduleGrid->addWidget(m_winterStart[school], row, 1);
        for (int term = 0; term < AcademicTermCount; ++term)
        {
            m_termWeeks[school][term] = new QSpinBox(schedules);
            m_termWeeks[school][term]->setRange(1,
                Application::MaximumTermWeeks);
            m_termWeeks[school][term]->setObjectName(
                QStringLiteral("calendarTermWeeks%1_%2").arg(school).arg(term));
            scheduleGrid->addWidget(m_termWeeks[school][term], row, term + 2);
        }
    }
    scheduleLayout->addLayout(scheduleGrid);
    outer->addWidget(schedules);

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("calendarPreferencesError"));
    m_error->setWordWrap(true);
    m_error->setStyleSheet(QStringLiteral("color: #a33;"));
    m_error->hide();
    outer->addWidget(m_error);

    auto* actionsRow = new QHBoxLayout;
    m_resetEvents = new QPushButton(preferencesDialogText("Reset Calendar to Defaults"), this);
    m_resetEvents->setObjectName(QStringLiteral("calendarResetEvents"));
    actionsRow->addWidget(m_resetEvents);
    actionsRow->addStretch(1);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("calendarPreferencesButtons"));
    actionsRow->addWidget(buttons);
    outer->addLayout(actionsRow);

    connect(buttons, &QDialogButtonBox::accepted,
        this, &CalendarPreferencesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_restoreSchedule, &QPushButton::clicked, this, [this]
    {
        if (!m_academicProvider)
        {
            return;
        }
        const int year = m_termYear->value();
        for (int school = 0; school < 2; ++school)
        {
            m_schedules[static_cast<std::size_t>(school)] =
                m_academicProvider->schedule().defaultYearSchedule(
                    schoolLevel(school), year);
        }
        for (int school = 0; school < 2; ++school)
        {
            const auto& schedule = m_schedules[static_cast<std::size_t>(school)];
            m_winterStart[school]->setDate(schedule.winterStart);
            for (int term = 0; term < AcademicTermCount; ++term)
            {
                m_termWeeks[school][term]->setValue(
                    schedule.weeks[static_cast<std::size_t>(term)]);
            }
        }
    });
    connect(m_termYear, qOverload<int>(&QSpinBox::valueChanged), this,
        [this](const int year)
        {
            if (!m_academicProvider)
            {
                return;
            }
            for (int school = 0; school < 2; ++school)
            {
                m_schedules[static_cast<std::size_t>(school)] =
                    m_academicProvider->schedule().yearSchedule(
                        schoolLevel(school), year);
            }
            for (int school = 0; school < 2; ++school)
            {
                const auto& schedule = m_schedules[static_cast<std::size_t>(school)];
                m_winterStart[school]->setDate(schedule.winterStart);
                for (int term = 0; term < AcademicTermCount; ++term)
                {
                    m_termWeeks[school][term]->setValue(
                        schedule.weeks[static_cast<std::size_t>(term)]);
                }
            }
        });
    connect(m_resetEvents, &QPushButton::clicked,
        this, &CalendarPreferencesDialog::resetCalendar);

    const auto display = m_actions.readDisplayPreferences
        ? m_actions.readDisplayPreferences(m_databasePath)
        : Application::CalendarEventDisplayPreferencesResult::failure({
              .code = Domain::ErrorCode::NotFound,
              .message = "Calendar preferences are unavailable.",
              .recoverable = true
          });
    if (display)
    {
        m_displayPreferences = display.value();
        m_displayPreferencesLoaded = true;
        m_showAllCampuses->setChecked(m_displayPreferences.showEventsAtAllCampuses);
        m_hideStartOfTerm->setChecked(m_displayPreferences.hideStartOfTermEvents);
    }
    else
    {
        m_showAllCampuses->setEnabled(false);
        m_hideStartOfTerm->setEnabled(false);
        m_error->setText(preferencesDialogText("Some preferences could not be loaded. Those controls are disabled."));
        m_error->show();
    }

    m_originalFirstDay = m_academicProvider
        ? m_academicProvider->firstDayOfWeek()
        : 0;
    m_firstDay->setCurrentIndex(m_firstDay->findData(m_originalFirstDay));
    m_firstDay->setEnabled(m_academicProvider
        && !m_academicProvider->firstDayOfWeekLoadError().has_value());
    const bool scheduleReady = m_academicProvider
        && m_academicProvider->schedulePersistenceKnown();
    m_termYear->setEnabled(scheduleReady);
    m_restoreSchedule->setEnabled(scheduleReady);
    for (int school = 0; school < 2; ++school)
    {
        m_schedules[static_cast<std::size_t>(school)] = m_academicProvider
            ? m_academicProvider->schedule().yearSchedule(
                  schoolLevel(school), m_termYear->value())
            : AcademicYearSchedule{};
        m_originalSchedules[static_cast<std::size_t>(school)] =
            m_schedules[static_cast<std::size_t>(school)];
        m_winterStart[school]->setEnabled(scheduleReady);
        m_winterStart[school]->setDate(m_schedules[static_cast<std::size_t>(school)].winterStart);
        for (int term = 0; term < AcademicTermCount; ++term)
        {
            m_termWeeks[school][term]->setEnabled(scheduleReady);
            m_termWeeks[school][term]->setValue(
                m_schedules[static_cast<std::size_t>(school)]
                    .weeks[static_cast<std::size_t>(term)]);
        }
    }

    for (int type = 0; type < static_cast<int>(EventTypes.size()); ++type)
    {
        if (m_actions.readEventTypeColor)
        {
            const auto color = m_actions.readEventTypeColor(
                m_databasePath, EventTypes.at(static_cast<std::size_t>(type)));
            if (color && QColor(utf8(color.value())).isValid())
            {
                m_colors[static_cast<std::size_t>(type)] = color.value();
            }
        }
        m_originalColors[static_cast<std::size_t>(type)] =
            m_colors[static_cast<std::size_t>(type)];
        updateColorButton(type);
    }
}

void CalendarPreferencesDialog::accept()
{
    if (saveChanges())
    {
        QDialog::accept();
    }
}

void CalendarPreferencesDialog::chooseColor(const int typeIndex)
{
    const QColor initial(utf8(m_colors.at(static_cast<std::size_t>(typeIndex))));
    const QColor selected = QColorDialog::getColor(
        initial.isValid() ? initial : QColor(defaultEventColor(typeIndex)),
        this,
        preferencesDialogText("Choose Event Color"));
    if (!selected.isValid())
    {
        return;
    }
    m_colors[static_cast<std::size_t>(typeIndex)] =
        selected.name(QColor::HexRgb).toStdString();
    updateColorButton(typeIndex);
}

void CalendarPreferencesDialog::resetCalendar()
{
    if (!m_actions.deleteAllEvents)
    {
        m_error->setText(preferencesDialogText("Calendar reset is unavailable."));
        m_error->show();
        return;
    }
    const auto answer = QMessageBox::question(
        this,
        preferencesDialogText("Reset Calendar?"),
        preferencesDialogText("This will permanently delete all calendar events and restore the calendar event list to defaults. This cannot be undone."),
        QMessageBox::Reset | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer != QMessageBox::Reset)
    {
        return;
    }
    const auto deleted = m_actions.deleteAllEvents(m_databasePath);
    if (!deleted)
    {
        m_error->setText(preferencesDialogText("Calendar events could not be reset. Your settings are still available."));
        m_error->show();
        return;
    }
    if (m_actions.preferencesChanged)
    {
        m_actions.preferencesChanged(true);
    }
}

bool CalendarPreferencesDialog::saveChanges()
{
    if (m_displayPreferencesLoaded)
    {
        const Application::CalendarEventDisplayPreferences request{
            .showEventsAtAllCampuses = m_showAllCampuses->isChecked(),
            .hideStartOfTermEvents = m_hideStartOfTerm->isChecked()
        };
        if (request != m_displayPreferences)
        {
            if (!m_actions.saveDisplayPreferences)
            {
                m_error->setText(preferencesDialogText("Calendar display preferences could not be saved."));
                m_error->show();
                return false;
            }
            const auto saved = m_actions.saveDisplayPreferences(m_databasePath, request);
            if (!saved)
            {
                m_error->setText(preferencesDialogText("Calendar display preferences could not be saved. Your edits are still available."));
                m_error->show();
                return false;
            }
            m_displayPreferences = request;
        }
    }

    const int selectedDay = m_firstDay->currentData().toInt();
    if (m_academicProvider && selectedDay != m_originalFirstDay)
    {
        const auto saved = m_academicProvider->setFirstDayOfWeek(selectedDay);
        if (!saved)
        {
            m_error->setText(preferencesDialogText("The first day of the week could not be saved. Your edits are still available."));
            m_error->show();
            return false;
        }
        m_originalFirstDay = selectedDay;
    }

    if (m_academicProvider && m_termYear->isEnabled())
    {
        std::array<AcademicYearSchedule, 2> edited;
        for (int school = 0; school < 2; ++school)
        {
            edited[static_cast<std::size_t>(school)] =
                m_schedules[static_cast<std::size_t>(school)];
            auto& schedule = edited[static_cast<std::size_t>(school)];
            schedule.termYear = m_termYear->value();
            schedule.winterStart = m_winterStart[school]->date();
            for (int term = 0; term < AcademicTermCount; ++term)
            {
                schedule.weeks[static_cast<std::size_t>(term)] =
                    m_termWeeks[school][term]->value();
            }
            if (!schedule.isValid())
            {
                m_error->setText(preferencesDialogText("Every term must start on a Monday and last between 1 and 53 weeks."));
                m_error->show();
                return false;
            }
        }
        const bool changed = !sameSchedule(edited[0], m_originalSchedules[0])
            || !sameSchedule(edited[1], m_originalSchedules[1])
            || m_termYear->value() != m_originalSchedules[0].termYear;
        if (changed)
        {
            if (m_termYear->value() > AcademicCalendarSchedule::FirstTermYear)
            {
                for (int school = 0; school < 2; ++school)
                {
                    const auto previous = m_academicProvider->schedule().yearSchedule(
                        schoolLevel(school), m_termYear->value() - 1);
                    if (!Application::termWeeksFromDayDistance(
                            previous.termStart(AcademicTerm::Fall).daysTo(
                                edited[static_cast<std::size_t>(school)].winterStart)))
                    {
                        m_error->setText(preferencesDialogText("Winter must begin 1 to 53 complete weeks after the preceding Fall term starts."));
                        m_error->show();
                        return false;
                    }
                }
            }
            if (m_academicProvider->hasCustomYearAfter(m_termYear->value()))
            {
                const auto answer = QMessageBox::question(
                    this,
                    preferencesDialogText("Replace Later Academic Schedules?"),
                    preferencesDialogText("Saving this year will replace custom schedules for later years. Continue?"),
                    QMessageBox::Yes | QMessageBox::Cancel,
                    QMessageBox::Cancel);
                if (answer != QMessageBox::Yes)
                {
                    return false;
                }
            }
            const auto saved = m_academicProvider->saveYearSchedules(
                m_termYear->value(), edited[0], edited[1]);
            if (!saved)
            {
                m_error->setText(preferencesDialogText("The academic schedule could not be saved. Your edits are still available."));
                m_error->show();
                return false;
            }
            m_schedules = edited;
            m_originalSchedules = edited;
        }
    }

    for (int type = 0; type < static_cast<int>(EventTypes.size()); ++type)
    {
        const auto index = static_cast<std::size_t>(type);
        if (m_colors[index] == m_originalColors[index])
        {
            continue;
        }
        if (!m_actions.saveEventTypeColor)
        {
            m_error->setText(preferencesDialogText("An event color could not be saved. Your edits are still available."));
            m_error->show();
            return false;
        }
        const auto saved = m_actions.saveEventTypeColor(
            m_databasePath, EventTypes[index], m_colors[index]);
        if (!saved)
        {
            m_error->setText(preferencesDialogText("An event color could not be saved. Your edits are still available."));
            m_error->show();
            return false;
        }
        m_originalColors[index] = m_colors[index];
    }

    m_error->hide();
    if (m_actions.preferencesChanged)
    {
        m_actions.preferencesChanged(false);
    }
    return true;
}

void CalendarPreferencesDialog::updateColorButton(const int typeIndex)
{
    const QColor color(utf8(m_colors.at(static_cast<std::size_t>(typeIndex))));
    m_colorButtons[typeIndex]->setText(color.name(QColor::HexRgb));
    m_colorButtons[typeIndex]->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;")
            .arg(color.name(QColor::HexRgb),
                 color.lightness() < 128 ? QStringLiteral("white")
                                         : QStringLiteral("black")));
}

} // namespace ClassMngr::Next::Ui

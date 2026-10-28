#pragma once

#include "features/calendar/academic_calendar_schedule.h"
#include "next/application/calendar_event_delete_all_port.h"
#include "next/application/calendar_event_display_preferences.h"
#include "next/application/calendar_event_type_color_preferences.h"

#include <QDialog>

#include <array>
#include <functional>
#include <map>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QSpinBox;

class AcademicCalendarProvider;

namespace ClassMngr::Next::Ui
{

struct CalendarPreferencesActions final
{
    std::function<Application::CalendarEventDisplayPreferencesResult(
        const QString&)> readDisplayPreferences;
    std::function<Application::CalendarEventDisplayPreferencesSaveResult(
        const QString&,
        const Application::CalendarEventDisplayPreferencesSaveRequest&)>
        saveDisplayPreferences;
    std::function<Application::CalendarEventDeleteAllResult(const QString&)>
        deleteAllEvents;
    std::function<Domain::Result<std::string>(
        const QString&, Domain::CalendarEventType)> readEventTypeColor;
    std::function<Application::CalendarEventTypeColorPreferencesPort::SaveResult(
        const QString&,
        Domain::CalendarEventType,
        const std::string&)> saveEventTypeColor;
    std::function<void(bool)> preferencesChanged;
};

class CalendarPreferencesDialog final : public QDialog
{
    Q_OBJECT

public:
    CalendarPreferencesDialog(
        QString databasePath,
        AcademicCalendarProvider* academicProvider,
        CalendarPreferencesActions actions,
        QWidget* parent = nullptr
        );

private:
    void accept() override;
    void chooseColor(int typeIndex);
    void resetCalendar();
    [[nodiscard]] bool saveChanges();
    void updateColorButton(int typeIndex);

    QString m_databasePath;
    AcademicCalendarProvider* m_academicProvider = nullptr;
    CalendarPreferencesActions m_actions;
    QCheckBox* m_showAllCampuses = nullptr;
    QCheckBox* m_hideStartOfTerm = nullptr;
    QComboBox* m_firstDay = nullptr;
    QSpinBox* m_termYear = nullptr;
    QDateEdit* m_winterStart[2]{};
    QSpinBox* m_termWeeks[2][AcademicTermCount]{};
    QPushButton* m_colorButtons[6]{};
    QPushButton* m_restoreSchedule = nullptr;
    QPushButton* m_resetEvents = nullptr;
    QLabel* m_error = nullptr;
    Application::CalendarEventDisplayPreferences m_displayPreferences;
    std::array<AcademicYearSchedule, 2> m_schedules;
    std::array<AcademicYearSchedule, 2> m_originalSchedules;
    std::array<std::string, 6> m_colors;
    std::array<std::string, 6> m_originalColors;
    int m_originalFirstDay = 0;
    bool m_displayPreferencesLoaded = false;
    bool m_saving = false;
};

} // namespace ClassMngr::Next::Ui

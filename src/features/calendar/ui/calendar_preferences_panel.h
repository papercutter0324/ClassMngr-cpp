#pragma once

#include "features/calendar/academic_calendar_schedule.h"

#include <array>

#include <QWidget>

class AcademicCalendarProvider;
class CalendarEventImportService;
class CalendarService;
class QCheckBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class SettingsService;

class CalendarPreferencesPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit CalendarPreferencesPanel(
        AcademicCalendarProvider* provider,
        CalendarService* calendarService,
        SettingsService* settingsService,
        QWidget* parent = nullptr
        );

signals:
    void calendarPreferencesChanged(bool eventsChanged);

private slots:
    void saveTermSchedules();
    void restoreDefaults();
    void resetCalendarEvents();
    void linkWinterSpring(bool linked);
    void importCalendarEvents();
    void handleImportFinished(
        int importedCount,
        int skippedCount
        );
    void handleImportFailed(const QString& message);
    void setTermYear(int termYear);

private:
    static constexpr int SchoolCount = 2;

    void buildUi();
    QWidget* buildOptionsSection();
    QWidget* buildTermSchedulesSection();
    QWidget* buildImportSection();
    void loadSchedules();
    void loadOptions();
    void saveOptions();
    void refreshFields();
    void commitDate(int schoolIndex, int termIndex);
    void clearDate(int schoolIndex, int termIndex);
    void commitWeeks(int schoolIndex, int termIndex);
    void synchronizeLinkedTerms();
    void updateLinkedFieldAvailability();
    [[nodiscard]] QString termName(int termIndex) const;

    AcademicCalendarProvider* m_provider = nullptr;
    CalendarService* m_calendarService = nullptr;
    SettingsService* m_settingsService = nullptr;
    CalendarEventImportService* m_importService = nullptr;
    int m_termYear = AcademicCalendarSchedule::FirstTermYear;
    bool m_refreshing = false;
    bool m_dirty = false;
    std::array<AcademicYearSchedule, SchoolCount> m_schedules;
    std::array<std::array<QDateEdit*, AcademicTermCount>, SchoolCount> m_dateEdits{};
    std::array<std::array<QSpinBox*, AcademicTermCount>, SchoolCount> m_weekEdits{};
    QCheckBox* m_showAllCampusesCheck = nullptr;
    QCheckBox* m_hideStartOfTermEventsCheck = nullptr;
    QCheckBox* m_startWeekOnMondayCheck = nullptr;
    QCheckBox* m_linkCheck = nullptr;
    QLineEdit* m_importUrlEdit = nullptr;
    QLabel* m_importStatusLabel = nullptr;
    QPushButton* m_importButton = nullptr;
    QPushButton* m_restoreButton = nullptr;
    QPushButton* m_resetEventsButton = nullptr;
    QPushButton* m_saveSchedulesButton = nullptr;
    QSpinBox* m_termYearSpin = nullptr;
};

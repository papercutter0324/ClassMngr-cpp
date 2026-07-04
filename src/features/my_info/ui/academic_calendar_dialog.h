#pragma once

#include "features/my_info/academic_calendar_schedule.h"

#include <QDialog>

#include <array>

class AcademicCalendarProvider;
class CalendarEventImportService;
class DataService;
class QCheckBox;
class QDateEdit;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

class AcademicCalendarDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AcademicCalendarDialog(
        AcademicCalendarProvider* provider,
        DataService* dataService,
        int termYear,
        QWidget* parent = nullptr
        );

signals:
    void calendarEventsImported();

private slots:
    void accept() override;
    void restoreDefaults();
    void resetCalendarEvents();
    void linkWinterSpring(bool linked);
    void importCalendarEvents();
    void handleImportFinished(
        int importedCount,
        int skippedCount
        );
    void handleImportFailed(
        const QString& message
        );

private:
    static constexpr int SchoolCount = 2;

    void buildUi();
    QWidget* buildOptionsTab();
    QWidget* buildTermSchedulesTab();
    QWidget* buildImportTab();
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
    DataService* m_dataService = nullptr;
    CalendarEventImportService* m_importService = nullptr;
    int m_termYear = AcademicCalendarSchedule::FirstTermYear;
    bool m_refreshing = false;
    bool m_dirty = false;
    std::array<AcademicYearSchedule, SchoolCount> m_schedules;
    std::array<std::array<QDateEdit*, AcademicTermCount>, SchoolCount> m_dateEdits{};
    std::array<std::array<QSpinBox*, AcademicTermCount>, SchoolCount> m_weekEdits{};
    QCheckBox* m_showAllCampusesCheck = nullptr;
    QCheckBox* m_linkCheck = nullptr;
    QLineEdit* m_importUrlEdit = nullptr;
    QLabel* m_importStatusLabel = nullptr;
    QPushButton* m_importButton = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_restoreButton = nullptr;
    QPushButton* m_resetEventsButton = nullptr;
};

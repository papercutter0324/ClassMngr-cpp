#pragma once

#include "features/my_info/academic_calendar_schedule.h"

#include <QDialog>

#include <array>

class AcademicCalendarProvider;
class QCheckBox;
class QDateEdit;
class QDialogButtonBox;
class QPushButton;
class QSpinBox;

class AcademicCalendarDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AcademicCalendarDialog(
        AcademicCalendarProvider* provider,
        int termYear,
        QWidget* parent = nullptr
        );

private slots:
    void accept() override;
    void restoreDefaults();
    void linkWinterSpring(bool linked);

private:
    static constexpr int SchoolCount = 2;

    void buildUi();
    void loadSchedules();
    void refreshFields();
    void commitDate(int schoolIndex, int termIndex);
    void clearDate(int schoolIndex, int termIndex);
    void commitWeeks(int schoolIndex, int termIndex);
    void synchronizeLinkedTerms();
    void updateLinkedFieldAvailability();
    [[nodiscard]] QString termName(int termIndex) const;

    AcademicCalendarProvider* m_provider = nullptr;
    int m_termYear = AcademicCalendarSchedule::FirstTermYear;
    bool m_refreshing = false;
    bool m_dirty = false;
    std::array<AcademicYearSchedule, SchoolCount> m_schedules;
    std::array<std::array<QDateEdit*, AcademicTermCount>, SchoolCount> m_dateEdits{};
    std::array<std::array<QSpinBox*, AcademicTermCount>, SchoolCount> m_weekEdits{};
    QCheckBox* m_linkCheck = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_restoreButton = nullptr;
};

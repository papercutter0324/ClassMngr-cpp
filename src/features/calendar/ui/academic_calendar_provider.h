#pragma once

#include "features/calendar/academic_calendar_schedule.h"
#include "next/application/academic_calendar_schedule_preferences.h"
#include "next/application/calendar_first_day_of_week_preferences.h"

#include <memory>

#include <QObject>
#include <QDateTime>
#include <QVariantList>

class AcademicCalendarProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(int firstDayOfWeek READ firstDayOfWeek NOTIFY firstDayOfWeekChanged)

public:
    explicit AcademicCalendarProvider(
        std::unique_ptr<ClassMngr::Next::Application::
            AcademicCalendarSchedulePreferencesPort> schedulePreferences,
        std::unique_ptr<ClassMngr::Next::Application::
            CalendarFirstDayOfWeekPreferencesPort> firstDayOfWeekPreferences,
        QObject* parent = nullptr
        );

    [[nodiscard]] int revision() const;
    [[nodiscard]] int firstDayOfWeek() const;
    [[nodiscard]] const AcademicCalendarSchedule& schedule() const;

    Q_INVOKABLE QString monthTitle(
        int year,
        int zeroBasedMonth
        ) const;
    Q_INVOKABLE QVariantList weekRows(
        int year,
        int zeroBasedMonth,
        int localeFirstDay
        ) const;
    Q_INVOKABLE int termYearForDate(
        const QDateTime& dateTime
        ) const;

    [[nodiscard]] int termYearForDate(const QDate& date) const;
    [[nodiscard]] bool hasCustomYearAfter(int termYear) const;
    void saveYearSchedules(
        int termYear,
        const AcademicYearSchedule& elementary,
        const AcademicYearSchedule& middle
        );
    void setFirstDayOfWeek(int firstDayOfWeek);
    void reload();

signals:
    void revisionChanged();
    void firstDayOfWeekChanged();

private:
    [[nodiscard]] QString termName(AcademicTerm term) const;
    [[nodiscard]] QString termWeekText(
        const AcademicTermPosition& position
        ) const;
    [[nodiscard]] QString tooltipText(
        SchoolLevel level,
        const AcademicTermPosition& position
        ) const;
    void loadOptions();
    void persist();
    void persistFirstDayOfWeek();

    std::unique_ptr<ClassMngr::Next::Application::
        AcademicCalendarSchedulePreferencesPort> m_schedulePreferences;
    std::unique_ptr<ClassMngr::Next::Application::
        CalendarFirstDayOfWeekPreferencesPort> m_firstDayOfWeekPreferences;
    AcademicCalendarSchedule m_schedule;
    int m_revision = 0;
    int m_firstDayOfWeek = 0;
};

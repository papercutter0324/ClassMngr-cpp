#pragma once

#include "features/calendar/academic_calendar_schedule.h"
#include "next/application/academic_calendar_schedule_preferences.h"
#include "next/application/calendar_first_day_of_week_preferences.h"

#include <memory>
#include <optional>

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
    [[nodiscard]] const std::optional<ClassMngr::Next::Domain::OperationError>&
        firstDayOfWeekLoadError() const;
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
    [[nodiscard]] ClassMngr::Next::Application::
        AcademicCalendarSchedulePreferencesSaveResult saveYearSchedules(
        int termYear,
        const AcademicYearSchedule& elementary,
        const AcademicYearSchedule& middle
        );
    [[nodiscard]] ClassMngr::Next::Application::
        CalendarFirstDayOfWeekPreferencesPort::SaveResult
        setFirstDayOfWeek(int firstDayOfWeek);
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
    [[nodiscard]] ClassMngr::Next::Application::
        AcademicCalendarSchedulePreferencesSaveResult persist(
            const AcademicCalendarSchedule& schedule
            ) const;

    std::unique_ptr<ClassMngr::Next::Application::
        AcademicCalendarSchedulePreferencesPort> m_schedulePreferences;
    std::unique_ptr<ClassMngr::Next::Application::
        CalendarFirstDayOfWeekPreferencesPort> m_firstDayOfWeekPreferences;
    AcademicCalendarSchedule m_schedule;
    std::optional<ClassMngr::Next::Domain::OperationError>
        m_scheduleReadError;
    std::optional<ClassMngr::Next::Domain::OperationError>
        m_firstDayOfWeekLoadError;
    bool m_schedulePersistenceKnown = false;
    int m_revision = 0;
    int m_firstDayOfWeek = 0;
};

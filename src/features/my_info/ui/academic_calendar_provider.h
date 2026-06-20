#pragma once

#include "features/my_info/academic_calendar_schedule.h"

#include <QObject>
#include <QDateTime>
#include <QVariantList>

class DataService;

class AcademicCalendarProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit AcademicCalendarProvider(
        DataService* dataService,
        QObject* parent = nullptr
        );

    [[nodiscard]] int revision() const;
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
    void reload();

signals:
    void revisionChanged();

private:
    [[nodiscard]] QString termName(AcademicTerm term) const;
    [[nodiscard]] QString termWeekText(
        const AcademicTermPosition& position
        ) const;
    [[nodiscard]] QString tooltipText(
        SchoolLevel level,
        const AcademicTermPosition& position
        ) const;
    void persist();

    DataService* m_dataService = nullptr;
    AcademicCalendarSchedule m_schedule;
    int m_revision = 0;
};

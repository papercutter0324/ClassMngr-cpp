#include "classmngr/engine/academic_calendar.h"

#include <algorithm>
#include <chrono>

namespace classmngr::engine
{
namespace
{
using std::chrono::days;
using std::chrono::sys_days;
using std::chrono::weekday;

constexpr int DaysPerWeek = 7;
constexpr int MaximumTermWeeks = 53;

int termIndex(AcademicTerm term)
{
    return static_cast<int>(term);
}

bool isMonday(const CalendarDate& date)
{
    return date.ok()
        && weekday{sys_days{date}}.iso_encoding() == 1;
}

sys_days toSysDays(const CalendarDate& date)
{
    return sys_days{date};
}

CalendarDate fromSysDays(const sys_days& date)
{
    return CalendarDate{date};
}

bool validScheduleMap(
    const AcademicCalendarSchedule::ScheduleMap& schedules
    )
{
    return std::ranges::all_of(
        schedules,
        [](const auto& entry)
        {
            return entry.first >= AcademicCalendarSchedule::FirstTermYear
                && entry.second.termYear == entry.first
                && entry.second.isValid();
        }
        );
}
} // namespace

bool AcademicYearSchedule::isValid() const
{
    if (
        termYear < AcademicCalendarSchedule::FirstTermYear
        || !isMonday(winterStart)
        )
    {
        return false;
    }

    return std::ranges::all_of(
        weeks,
        [](int count)
        {
            return count >= 1 && count <= MaximumTermWeeks;
        }
        );
}

CalendarDate AcademicYearSchedule::termStart(AcademicTerm term) const
{
    const int index = termIndex(term);
    if (!isValid() || index < 0 || index >= AcademicTermCount)
    {
        return {};
    }

    sys_days start = toSysDays(winterStart);
    for (int previous = 0; previous < index; ++previous)
    {
        start += days{weeks[previous] * DaysPerWeek};
    }

    return fromSysDays(start);
}

CalendarDate AcademicYearSchedule::endDate() const
{
    if (!isValid())
    {
        return {};
    }

    int totalWeeks = 0;
    for (const int count : weeks)
    {
        totalWeeks += count;
    }

    return fromSysDays(
        toSysDays(winterStart) + days{totalWeeks * DaysPerWeek}
        );
}

CalendarDate AcademicCalendarSchedule::initialWinterStart()
{
    return CalendarDate{
        std::chrono::year{2025},
        std::chrono::month{12},
        std::chrono::day{29}
    };
}

std::array<int, AcademicTermCount>
AcademicCalendarSchedule::defaultWeeks(SchoolLevel level)
{
    if (level == SchoolLevel::Elementary)
    {
        return {11, 19, 11, 11};
    }

    return {11, 19, 4, 18};
}

AcademicYearSchedule AcademicCalendarSchedule::yearSchedule(
    SchoolLevel level,
    int termYear
    ) const
{
    if (termYear < FirstTermYear)
    {
        return {};
    }

    const ScheduleMap& custom = schedules(level);
    AcademicYearSchedule current{
        FirstTermYear,
        initialWinterStart(),
        defaultWeeks(level)
    };

    for (int year = FirstTermYear; year <= termYear; ++year)
    {
        if (year > FirstTermYear)
        {
            current = {
                year,
                current.endDate(),
                defaultWeeks(level)
            };
        }

        const auto found = custom.find(year);
        if (found != custom.end())
        {
            current = found->second;
        }
    }

    return current;
}

AcademicYearSchedule AcademicCalendarSchedule::defaultYearSchedule(
    SchoolLevel level,
    int termYear
    ) const
{
    if (termYear < FirstTermYear)
    {
        return {};
    }

    const CalendarDate winterStart =
        termYear == FirstTermYear
            ? initialWinterStart()
            : yearSchedule(level, termYear - 1).endDate();

    return {
        termYear,
        winterStart,
        defaultWeeks(level)
    };
}

AcademicTermPosition AcademicCalendarSchedule::termAt(
    SchoolLevel level,
    const CalendarDate& date
    ) const
{
    if (!date.ok() || toSysDays(date) < toSysDays(initialWinterStart()))
    {
        return {};
    }

    AcademicYearSchedule schedule = yearSchedule(level, FirstTermYear);
    int termYear = FirstTermYear;

    while (toSysDays(date) >= toSysDays(schedule.endDate()))
    {
        ++termYear;
        schedule = yearSchedule(level, termYear);
    }

    for (int index = AcademicTermCount - 1; index >= 0; --index)
    {
        const AcademicTerm term = static_cast<AcademicTerm>(index);
        const CalendarDate start = schedule.termStart(term);
        if (toSysDays(date) >= toSysDays(start))
        {
            const auto elapsed =
                (toSysDays(date) - toSysDays(start)).count();
            const int week = static_cast<int>(elapsed / DaysPerWeek) + 1;
            return {
                true,
                termYear,
                term,
                week,
                fromSysDays(
                    toSysDays(start) + days{(week - 1) * DaysPerWeek}
                    )
            };
        }
    }

    return {};
}

bool AcademicCalendarSchedule::hasCustomYearAfter(int termYear) const
{
    const auto hasLater = [termYear](const ScheduleMap& schedules)
    {
        return schedules.upper_bound(termYear) != schedules.end();
    };

    return hasLater(m_elementarySchedules)
        || hasLater(m_middleSchedules);
}

bool AcademicCalendarSchedule::hasSavedSchedules() const
{
    return !m_elementarySchedules.empty()
        && !m_middleSchedules.empty();
}

void AcademicCalendarSchedule::setYearSchedules(
    int termYear,
    const AcademicYearSchedule& elementary,
    const AcademicYearSchedule& middle
    )
{
    if (
        termYear < FirstTermYear
        || !elementary.isValid()
        || !middle.isValid()
        || elementary.termYear != termYear
        || middle.termYear != termYear
        )
    {
        return;
    }

    AcademicYearSchedule previousElementary;
    AcademicYearSchedule previousMiddle;

    if (termYear > FirstTermYear)
    {
        previousElementary =
            yearSchedule(SchoolLevel::Elementary, termYear - 1);
        previousMiddle =
            yearSchedule(SchoolLevel::Middle, termYear - 1);

        const auto alignPreviousFall =
            [](AcademicYearSchedule& previous,
               const CalendarDate& nextWinterStart)
        {
            const CalendarDate fallStart =
                previous.termStart(AcademicTerm::Fall);
            const int elapsed = static_cast<int>(
                (toSysDays(nextWinterStart) - toSysDays(fallStart)).count()
                );
            const int weeks = elapsed / DaysPerWeek;

            if (
                elapsed <= 0
                || elapsed % DaysPerWeek != 0
                || weeks > MaximumTermWeeks
                )
            {
                return false;
            }

            previous.weeks[termIndex(AcademicTerm::Fall)] = weeks;
            return true;
        };

        if (
            !alignPreviousFall(previousElementary, elementary.winterStart)
            || !alignPreviousFall(previousMiddle, middle.winterStart)
            )
        {
            return;
        }
    }

    while (
        !m_elementarySchedules.empty()
        && m_elementarySchedules.rbegin()->first > termYear
        )
    {
        m_elementarySchedules.erase(
            std::prev(m_elementarySchedules.end())
            );
    }

    while (
        !m_middleSchedules.empty()
        && m_middleSchedules.rbegin()->first > termYear
        )
    {
        m_middleSchedules.erase(
            std::prev(m_middleSchedules.end())
            );
    }

    if (termYear > FirstTermYear)
    {
        m_elementarySchedules.insert_or_assign(
            termYear - 1,
            previousElementary
            );
        m_middleSchedules.insert_or_assign(
            termYear - 1,
            previousMiddle
            );
    }

    m_elementarySchedules.insert_or_assign(termYear, elementary);
    m_middleSchedules.insert_or_assign(termYear, middle);
}

bool AcademicCalendarSchedule::replaceSchedules(
    const ScheduleMap& elementary,
    const ScheduleMap& middle
    )
{
    if (!validScheduleMap(elementary) || !validScheduleMap(middle))
    {
        return false;
    }

    m_elementarySchedules = elementary;
    m_middleSchedules = middle;
    return true;
}

const AcademicCalendarSchedule::ScheduleMap&
AcademicCalendarSchedule::customSchedules(SchoolLevel level) const
{
    return schedules(level);
}

void AcademicCalendarSchedule::clear()
{
    m_elementarySchedules.clear();
    m_middleSchedules.clear();
}

const AcademicCalendarSchedule::ScheduleMap&
AcademicCalendarSchedule::schedules(SchoolLevel level) const
{
    return level == SchoolLevel::Elementary
        ? m_elementarySchedules
        : m_middleSchedules;
}

AcademicCalendarSchedule::ScheduleMap&
AcademicCalendarSchedule::schedules(SchoolLevel level)
{
    return level == SchoolLevel::Elementary
        ? m_elementarySchedules
        : m_middleSchedules;
}

} // namespace classmngr::engine

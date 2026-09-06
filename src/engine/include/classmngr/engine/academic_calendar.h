#pragma once

#include <array>
#include <chrono>
#include <map>

namespace classmngr::engine
{

enum class SchoolLevel
{
    Elementary,
    Middle
};

enum class AcademicTerm
{
    Winter,
    Spring,
    Summer,
    Fall
};

inline constexpr int AcademicTermCount = 4;
using CalendarDate = std::chrono::year_month_day;

struct AcademicYearSchedule
{
    int termYear = 0;
    CalendarDate winterStart{};
    std::array<int, AcademicTermCount> weeks{};

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] CalendarDate termStart(AcademicTerm term) const;
    [[nodiscard]] CalendarDate endDate() const;
};

struct AcademicTermPosition
{
    bool valid = false;
    int termYear = 0;
    AcademicTerm term = AcademicTerm::Winter;
    int week = 0;
    CalendarDate weekStart{};
};

class AcademicCalendarSchedule
{
public:
    static constexpr int FirstTermYear = 2026;
    using ScheduleMap = std::map<int, AcademicYearSchedule>;

    [[nodiscard]] static CalendarDate initialWinterStart();
    [[nodiscard]] static std::array<int, AcademicTermCount> defaultWeeks(
        SchoolLevel level
        );

    [[nodiscard]] AcademicYearSchedule yearSchedule(
        SchoolLevel level,
        int termYear
        ) const;
    [[nodiscard]] AcademicYearSchedule defaultYearSchedule(
        SchoolLevel level,
        int termYear
        ) const;
    [[nodiscard]] AcademicTermPosition termAt(
        SchoolLevel level,
        const CalendarDate& date
        ) const;

    [[nodiscard]] bool hasCustomYearAfter(int termYear) const;
    [[nodiscard]] bool hasSavedSchedules() const;
    void setYearSchedules(
        int termYear,
        const AcademicYearSchedule& elementary,
        const AcademicYearSchedule& middle
        );

    // Used by presentation adapters when loading the existing settings
    // format.  Validation stays in the portable schedule model.
    [[nodiscard]] bool replaceSchedules(
        const ScheduleMap& elementary,
        const ScheduleMap& middle
        );
    [[nodiscard]] const ScheduleMap& customSchedules(
        SchoolLevel level
        ) const;

    void clear();

private:
    [[nodiscard]] const ScheduleMap& schedules(SchoolLevel level) const;
    [[nodiscard]] ScheduleMap& schedules(SchoolLevel level);

    ScheduleMap m_elementarySchedules;
    ScheduleMap m_middleSchedules;
};

} // namespace classmngr::engine

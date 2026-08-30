#include "classmngr/engine/academic_calendar.h"

#include <chrono>
#include <iostream>
#include <string_view>

namespace
{
using classmngr::engine::AcademicCalendarSchedule;
using classmngr::engine::AcademicTerm;
using classmngr::engine::AcademicYearSchedule;
using classmngr::engine::CalendarDate;
using classmngr::engine::SchoolLevel;

CalendarDate date(int year, unsigned month, unsigned day)
{
    return {
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
}

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineAcademicCalendarTests: "
              << message
              << '\n';
    return false;
}

bool sameDate(const CalendarDate& left, const CalendarDate& right)
{
    return left == right;
}
} // namespace

int main()
{
    AcademicCalendarSchedule calendar;
    bool passed = true;

    const AcademicYearSchedule elementary =
        calendar.yearSchedule(SchoolLevel::Elementary, 2026);
    const AcademicYearSchedule middle =
        calendar.yearSchedule(SchoolLevel::Middle, 2026);

    passed &= expect(
        sameDate(elementary.winterStart, date(2025, 12, 29))
            && sameDate(
                elementary.termStart(AcademicTerm::Spring),
                date(2026, 3, 16)
                )
            && sameDate(
                elementary.termStart(AcademicTerm::Summer),
                date(2026, 7, 27)
                )
            && sameDate(
                elementary.termStart(AcademicTerm::Fall),
                date(2026, 10, 12)
                )
            && sameDate(elementary.endDate(), date(2026, 12, 28))
            && sameDate(
                middle.termStart(AcademicTerm::Fall),
                date(2026, 8, 24)
                ),
        "default term boundaries changed"
        );

    const auto june = calendar.termAt(
        SchoolLevel::Elementary,
        date(2026, 6, 1)
        );
    const auto elementarySeptember = calendar.termAt(
        SchoolLevel::Elementary,
        date(2026, 9, 7)
        );
    const auto middleSeptember = calendar.termAt(
        SchoolLevel::Middle,
        date(2026, 9, 7)
        );
    passed &= expect(
        june.valid
            && june.term == AcademicTerm::Spring
            && june.week == 12
            && sameDate(june.weekStart, date(2026, 6, 1))
            && elementarySeptember.term == AcademicTerm::Summer
            && elementarySeptember.week == 7
            && middleSeptember.term == AcademicTerm::Fall
            && middleSeptember.week == 3,
        "term lookup did not reset week numbers at term boundaries"
        );

    AcademicYearSchedule revisedElementary = elementary;
    AcademicYearSchedule revisedMiddle = middle;
    revisedElementary.weeks[2] = 12;
    calendar.setYearSchedules(2026, revisedElementary, revisedMiddle);
    const auto elementary2027 = calendar.yearSchedule(
        SchoolLevel::Elementary,
        2027
        );
    const auto middle2027 = calendar.yearSchedule(
        SchoolLevel::Middle,
        2027
        );
    passed &= expect(
        sameDate(elementary2027.winterStart, date(2027, 1, 4))
            && elementary2027.weeks
                == AcademicCalendarSchedule::defaultWeeks(
                    SchoolLevel::Elementary
                    )
            && sameDate(middle2027.winterStart, date(2026, 12, 28)),
        "custom term lengths did not shift future defaults"
        );

    AcademicYearSchedule elementary2028 = calendar.yearSchedule(
        SchoolLevel::Elementary,
        2028
        );
    AcademicYearSchedule middle2028 = calendar.yearSchedule(
        SchoolLevel::Middle,
        2028
        );
    elementary2028.winterStart = date(2028, 1, 17);
    middle2028.winterStart = date(2028, 1, 3);
    calendar.setYearSchedules(2028, elementary2028, middle2028);
    passed &= expect(
        calendar.yearSchedule(SchoolLevel::Elementary, 2027).weeks[3] == 13
            && calendar.yearSchedule(SchoolLevel::Middle, 2027).weeks[3] == 19,
        "editing winter start did not keep the previous fall continuous"
        );

    const auto customElementary = calendar.customSchedules(
        SchoolLevel::Elementary
        );
    passed &= expect(
        customElementary.find(2027) != customElementary.end()
            && customElementary.find(2028) != customElementary.end()
            && calendar.hasSavedSchedules()
            && !calendar.hasCustomYearAfter(2028),
        "custom schedule snapshot did not retain saved years"
        );

    AcademicCalendarSchedule restored;
    passed &= expect(
        restored.replaceSchedules(
            calendar.customSchedules(SchoolLevel::Elementary),
            calendar.customSchedules(SchoolLevel::Middle)
            )
            && restored.yearSchedule(
                SchoolLevel::Elementary,
                2028
                ).weeks[0] == elementary2028.weeks[0],
        "custom schedule replacement failed"
        );

    auto invalid = customElementary;
    invalid.at(2028).weeks[0] = 0;
    passed &= expect(
        !restored.replaceSchedules(
            invalid,
            calendar.customSchedules(SchoolLevel::Middle)
            )
            && restored.yearSchedule(
                SchoolLevel::Elementary,
                2028
                ).weeks[0] == elementary2028.weeks[0],
        "invalid replacement changed the active schedule"
        );

    return passed ? 0 : 1;
}

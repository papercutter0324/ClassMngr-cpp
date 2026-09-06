#include "classmngr/engine/upcoming_birthday_schedule.h"

#include <chrono>
#include <iostream>
#include <string_view>
#include <utility>

namespace
{
using namespace classmngr::engine;

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineUpcomingBirthdayScheduleTests: "
              << message << '\n';
    return false;
}

CalendarDate date(int year, unsigned month, unsigned day)
{
    return {
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
}

Teacher teacher(std::string name, std::string birthday)
{
    Teacher result;
    result.teacherEn = std::move(name);
    result.birthday = std::move(birthday);
    return result;
}

NativeEnglishTeacher nativeTeacher(
    std::string name,
    std::string position,
    std::string birthday
    )
{
    NativeEnglishTeacher result;
    result.name = std::move(name);
    result.position = std::move(position);
    result.birthday = std::move(birthday);
    return result;
}

GsTeamMember gsTeamMember(
    std::string name,
    std::string koreanName,
    std::string position,
    std::string birthday
    )
{
    GsTeamMember result;
    result.name = std::move(name);
    result.koreanName = std::move(koreanName);
    result.position = std::move(position);
    result.birthday = std::move(birthday);
    return result;
}
} // namespace

int main()
{
    bool passed = true;

    const auto schedule = UpcomingBirthdaySchedule::build(
        {teacher("Alex", "08-21")},
        {nativeTeacher("Blair", "NET", "08-22")},
        {gsTeamMember("Casey", "케이시", "M3", "08-24")},
        date(2026, 8, 21)
        );

    passed &= expect(
        schedule.today.size() == 1
            && schedule.today.front().displayName == "Alex"
            && schedule.today.front().group
                == UpcomingBirthdayGroup::KoreanTeacher,
        "staff birthdays were not separated into today"
        );
    passed &= expect(
        schedule.thisWeek.size() == 1
            && schedule.thisWeek.front().date == date(2026, 8, 22)
            && schedule.thisWeek.front().position == "NET"
            && schedule.thisWeek.front().group
                == UpcomingBirthdayGroup::NativeEnglishTeacher,
        "this-week birthday was not preserved"
        );
    passed &= expect(
        schedule.nextWeek.size() == 1
            && schedule.nextWeek.front().date == date(2026, 8, 24)
            && schedule.nextWeek.front().position == "M3"
            && schedule.nextWeek.front().group
                == UpcomingBirthdayGroup::GsTeam,
        "next-week birthday was not preserved"
        );

    const auto filtered = UpcomingBirthdaySchedule::build(
        {
            teacher("Zara", "08-18"),
            teacher("Bella", "08-19"),
            teacher("Alex", "08-19"),
            teacher("", "08-20"),
            teacher("Invalid", "02-30")
        },
        {},
        {gsTeamMember("", "한국 이름", "Branch Manager", "08-20")},
        date(2026, 8, 19)
        );
    passed &= expect(
        filtered.today.size() == 2
            && filtered.today.at(0).displayName == "Alex"
            && filtered.today.at(1).displayName == "Bella"
            && filtered.thisWeek.size() == 1
            && filtered.thisWeek.front().displayName == "한국 이름"
            && filtered.nextWeek.empty(),
        "invalid, past, or same-day birthday ordering changed"
        );

    const auto crossYear = UpcomingBirthdaySchedule::build(
        {
            teacher("This Week", "01-03"),
            teacher("Next Week", "01-04")
        },
        {},
        {},
        date(2026, 12, 28)
        );
    passed &= expect(
        crossYear.thisWeek.size() == 1
            && crossYear.thisWeek.front().date == date(2027, 1, 3)
            && crossYear.nextWeek.size() == 1
            && crossYear.nextWeek.front().date == date(2027, 1, 4),
        "birthday window did not cross the calendar year"
        );

    const auto leapDay = UpcomingBirthdaySchedule::build(
        {teacher("Leap Day", "02-29")},
        {},
        {},
        date(2027, 2, 22)
        );
    passed &= expect(
        leapDay.thisWeek.size() == 1
            && leapDay.thisWeek.front().date == date(2027, 2, 28),
        "leap-day fallback changed"
        );

    const auto invalidReference = UpcomingBirthdaySchedule::build(
        {teacher("Alex", "08-21")},
        {},
        {},
        date(2026, 2, 30)
        );
    passed &= expect(
        invalidReference.isEmpty(),
        "invalid reference date was not rejected"
        );

    return passed ? 0 : 1;
}

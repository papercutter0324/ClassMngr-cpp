#include "classmngr/engine/upcoming_birthday_schedule.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

namespace classmngr::engine
{
namespace
{
using std::chrono::days;
using std::chrono::month;
using std::chrono::sys_days;
using std::chrono::year;
using std::chrono::year_month_day;

struct BirthdayParts
{
    unsigned month = 0;
    unsigned day = 0;
};

std::string trimAsciiWhitespace(
    std::string_view value
    )
{
    const auto isWhitespace = [](char character)
    {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    };

    std::size_t first = 0;
    while (first < value.size() && isWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

bool isAsciiDigit(char character)
{
    return character >= '0' && character <= '9';
}

std::optional<BirthdayParts> parseBirthday(
    std::string_view value
    )
{
    const std::string normalized = trimAsciiWhitespace(value);
    if (
        normalized.size() != 5
        || normalized[2] != '-'
        || !isAsciiDigit(normalized[0])
        || !isAsciiDigit(normalized[1])
        || !isAsciiDigit(normalized[3])
        || !isAsciiDigit(normalized[4])
        )
    {
        return std::nullopt;
    }

    const unsigned birthdayMonth = static_cast<unsigned>(
        (normalized[0] - '0') * 10 + normalized[1] - '0'
        );
    const unsigned birthdayDay = static_cast<unsigned>(
        (normalized[3] - '0') * 10 + normalized[4] - '0'
        );
    const year_month_day validationDate{
        year{2000},
        month{birthdayMonth},
        std::chrono::day{birthdayDay}
    };

    if (!validationDate.ok())
    {
        return std::nullopt;
    }

    return BirthdayParts{birthdayMonth, birthdayDay};
}

std::optional<CalendarDate> occurrenceForYear(
    const BirthdayParts& birthday,
    int occurrenceYear
    )
{
    CalendarDate occurrence{
        year{occurrenceYear},
        month{birthday.month},
        std::chrono::day{birthday.day}
    };

    if (occurrence.ok())
    {
        return occurrence;
    }

    if (birthday.month == 2 && birthday.day == 29)
    {
        return CalendarDate{
            year{occurrenceYear},
            month{2},
            std::chrono::day{28}
        };
    }

    return std::nullopt;
}

std::optional<CalendarDate> nextOccurrenceInRange(
    const BirthdayParts& birthday,
    const CalendarDate& rangeStart,
    const CalendarDate& rangeEnd
    )
{
    const int firstYear = static_cast<int>(rangeStart.year());
    const int lastYear = static_cast<int>(rangeEnd.year());
    for (int occurrenceYear = firstYear;
         occurrenceYear <= lastYear;
         ++occurrenceYear)
    {
        const auto occurrence = occurrenceForYear(
            birthday,
            occurrenceYear
            );
        if (
            occurrence.has_value()
            && sys_days{*occurrence} >= sys_days{rangeStart}
            && sys_days{*occurrence} <= sys_days{rangeEnd}
            )
        {
            return occurrence;
        }
    }

    return std::nullopt;
}

CalendarDate weekEnd(
    const CalendarDate& referenceDate,
    unsigned additionalDays
    )
{
    return CalendarDate{
        sys_days{referenceDate} + days{additionalDays}
    };
}

int compareText(
    std::string_view left,
    std::string_view right
    )
{
    if (left < right)
    {
        return -1;
    }
    if (left > right)
    {
        return 1;
    }
    return 0;
}

bool birthdayLessThan(
    const UpcomingBirthday& left,
    const UpcomingBirthday& right
    )
{
    const sys_days leftDate{left.date};
    const sys_days rightDate{right.date};
    if (leftDate != rightDate)
    {
        return leftDate < rightDate;
    }

    const int nameComparison = compareText(
        left.displayName,
        right.displayName
        );
    if (nameComparison != 0)
    {
        return nameComparison < 0;
    }

    if (left.group != right.group)
    {
        return static_cast<int>(left.group)
            < static_cast<int>(right.group);
    }

    return compareText(left.position, right.position) < 0;
}

std::optional<UpcomingBirthday> birthdayInRange(
    std::string_view birthdayValue,
    std::string displayNameValue,
    std::string position,
    UpcomingBirthdayGroup group,
    const CalendarDate& referenceDate,
    const CalendarDate& thisWeekEnd,
    const CalendarDate& nextWeekEnd
    )
{
    const auto birthday = parseBirthday(birthdayValue);
    const std::string displayName = trimAsciiWhitespace(displayNameValue);
    if (!birthday.has_value() || displayName.empty())
    {
        return std::nullopt;
    }

    const auto occurrence = nextOccurrenceInRange(
        *birthday,
        referenceDate,
        nextWeekEnd
        );
    if (!occurrence.has_value())
    {
        return std::nullopt;
    }

    const UpcomingBirthday entry{
        *occurrence,
        displayName,
        trimAsciiWhitespace(position),
        group
    };

    return entry;
}

void appendToRange(
    UpcomingBirthdaySchedule* schedule,
    const UpcomingBirthday& entry,
    const CalendarDate& referenceDate,
    const CalendarDate& thisWeekEnd
    )
{
    if (schedule == nullptr)
    {
        return;
    }

    if (entry.date == referenceDate)
    {
        schedule->today.push_back(entry);
    }
    else if (sys_days{entry.date} <= sys_days{thisWeekEnd})
    {
        schedule->thisWeek.push_back(entry);
    }
    else
    {
        schedule->nextWeek.push_back(entry);
    }
}

void appendBirthdayToSchedule(
    UpcomingBirthdaySchedule* schedule,
    std::string_view birthdayValue,
    std::string displayNameValue,
    std::string position,
    UpcomingBirthdayGroup group,
    const CalendarDate& referenceDate,
    const CalendarDate& thisWeekEnd,
    const CalendarDate& nextWeekEnd
    )
{
    const auto entry = birthdayInRange(
        birthdayValue,
        std::move(displayNameValue),
        std::move(position),
        group,
        referenceDate,
        thisWeekEnd,
        nextWeekEnd
        );
    if (entry.has_value())
    {
        appendToRange(
            schedule,
            *entry,
            referenceDate,
            thisWeekEnd
            );
    }
}

void sortBirthdays(std::vector<UpcomingBirthday>* birthdays)
{
    if (birthdays != nullptr)
    {
        std::sort(birthdays->begin(), birthdays->end(), birthdayLessThan);
    }
}
} // namespace

bool UpcomingBirthdaySchedule::isEmpty() const
{
    return today.empty() && thisWeek.empty() && nextWeek.empty();
}

UpcomingBirthdaySchedule UpcomingBirthdaySchedule::build(
    const std::vector<Teacher>& teachers,
    const std::vector<NativeEnglishTeacher>& nativeEnglishTeachers,
    const std::vector<GsTeamMember>& gsTeamMembers,
    const CalendarDate& referenceDate
    )
{
    UpcomingBirthdaySchedule result;

    if (!referenceDate.ok())
    {
        return result;
    }

    const unsigned isoWeekday = std::chrono::weekday{
        sys_days{referenceDate}
    }.iso_encoding();
    const CalendarDate thisWeekEnd = weekEnd(
        referenceDate,
        7U - isoWeekday
        );
    const CalendarDate nextWeekEnd = weekEnd(
        thisWeekEnd,
        7
        );

    for (const Teacher& teacher : teachers)
    {
        appendBirthdayToSchedule(
            &result,
            teacher.birthday,
            teacher.preferredDisplayName(),
            {},
            UpcomingBirthdayGroup::KoreanTeacher,
            referenceDate,
            thisWeekEnd,
            nextWeekEnd
            );
    }

    for (const NativeEnglishTeacher& teacher : nativeEnglishTeachers)
    {
        appendBirthdayToSchedule(
            &result,
            teacher.birthday,
            teacher.name,
            teacher.position,
            UpcomingBirthdayGroup::NativeEnglishTeacher,
            referenceDate,
            thisWeekEnd,
            nextWeekEnd
            );
    }

    for (const GsTeamMember& member : gsTeamMembers)
    {
        const std::string displayName =
            !trimAsciiWhitespace(member.name).empty()
                ? member.name
                : member.koreanName;
        appendBirthdayToSchedule(
            &result,
            member.birthday,
            displayName,
            member.position,
            UpcomingBirthdayGroup::GsTeam,
            referenceDate,
            thisWeekEnd,
            nextWeekEnd
            );
    }

    sortBirthdays(&result.today);
    sortBirthdays(&result.thisWeek);
    sortBirthdays(&result.nextWeek);
    return result;
}

} // namespace classmngr::engine

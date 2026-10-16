#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "qt_compatible_text.h"

namespace ClassMngr::Next::Application
{

struct UpcomingBirthdayDate final
{
    int year = 0;
    int month = 0;
    int day = 0;

    friend bool operator==(
        const UpcomingBirthdayDate&,
        const UpcomingBirthdayDate&
        ) = default;
};

enum class UpcomingBirthdayStaffGroup
{
    KoreanTeacher,
    NativeEnglishTeacher,
    GsTeam
};

struct UpcomingBirthdayCandidate final
{
    std::string birthdayMonthDay;
    std::u16string displayName;
    std::u16string position;
    UpcomingBirthdayStaffGroup group = UpcomingBirthdayStaffGroup::KoreanTeacher;
};

struct UpcomingBirthdayOccurrence final
{
    UpcomingBirthdayDate date;
    std::u16string displayName;
    std::u16string position;
    UpcomingBirthdayStaffGroup group = UpcomingBirthdayStaffGroup::KoreanTeacher;
};

struct UpcomingBirthdaySchedule final
{
    std::vector<UpcomingBirthdayOccurrence> today;
    std::vector<UpcomingBirthdayOccurrence> thisWeek;
    std::vector<UpcomingBirthdayOccurrence> nextWeek;

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return today.empty() && thisWeek.empty() && nextWeek.empty();
    }
};

// Returns the Qt-compatible locale comparison ordering for two UTF-16 views.
// The feature adapter supplies the implementation so this contract stays Qt-free.
using UpcomingBirthdayTextComparator = int(
    std::u16string_view,
    std::u16string_view
    );

// Date parsing, yearly occurrence selection, and week bucketing are independent
// of Qt. The feature adapter remains responsible for source models and text
// conversion, and supplies the locale-aware text comparison used for ordering.
class UpcomingBirthdayScheduleUseCase final
{
public:
    [[nodiscard]] static UpcomingBirthdaySchedule build(
        const std::vector<UpcomingBirthdayCandidate>& candidates,
        const UpcomingBirthdayDate& referenceDate,
        UpcomingBirthdayTextComparator& compareText
        )
    {
        using namespace std::chrono;

        UpcomingBirthdaySchedule schedule;
        const std::optional<sys_days> reference = toSysDays(referenceDate);
        if (!reference)
        {
            return schedule;
        }

        const int dayOfWeek = weekday{*reference}.iso_encoding();
        constexpr sys_days maximumDate = sys_days{
            year{9999} / month{12} / day{31}
        };
        const sys_days thisWeekEnd = std::min(
            *reference + days{7 - dayOfWeek},
            maximumDate
            );
        const sys_days nextWeekEnd = std::min(
            thisWeekEnd + days{7},
            maximumDate
            );

        for (const UpcomingBirthdayCandidate& candidate : candidates)
        {
            if (!isKnownGroup(candidate.group))
            {
                continue;
            }

            if (isBlank(candidate.displayName))
            {
                continue;
            }

            const std::optional<std::pair<int, int>> birthday =
                parseBirthday(candidate.birthdayMonthDay);
            if (!birthday)
            {
                continue;
            }

            const std::optional<sys_days> occurrence = nextOccurrence(
                birthday->first,
                birthday->second,
                *reference,
                nextWeekEnd
                );
            if (!occurrence)
            {
                continue;
            }

            UpcomingBirthdayOccurrence entry{
                .date = fromSysDays(*occurrence),
                .displayName = candidate.displayName,
                .position = candidate.position,
                .group = candidate.group
            };

            if (*occurrence == *reference)
            {
                schedule.today.push_back(std::move(entry));
            }
            else if (*occurrence <= thisWeekEnd)
            {
                schedule.thisWeek.push_back(std::move(entry));
            }
            else
            {
                schedule.nextWeek.push_back(std::move(entry));
            }
        }

        sortOccurrences(&schedule.today, compareText);
        sortOccurrences(&schedule.thisWeek, compareText);
        sortOccurrences(&schedule.nextWeek, compareText);

        return schedule;
    }

private:
    using SysDays = std::chrono::sys_days;

    static void sortOccurrences(
        std::vector<UpcomingBirthdayOccurrence>* occurrences,
        UpcomingBirthdayTextComparator& compareText
        )
    {
        if (!occurrences)
        {
            return;
        }

        std::sort(
            occurrences->begin(),
            occurrences->end(),
            [&compareText](
                const UpcomingBirthdayOccurrence& left,
                const UpcomingBirthdayOccurrence& right
                )
            {
                if (left.date.year != right.date.year)
                {
                    return left.date.year < right.date.year;
                }
                if (left.date.month != right.date.month)
                {
                    return left.date.month < right.date.month;
                }
                if (left.date.day != right.date.day)
                {
                    return left.date.day < right.date.day;
                }

                const int nameComparison = compareText(
                    std::u16string_view(left.displayName),
                    std::u16string_view(right.displayName)
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

                return compareText(
                    std::u16string_view(left.position),
                    std::u16string_view(right.position)
                    ) < 0;
            }
            );
    }

    [[nodiscard]] static bool isKnownGroup(
        const UpcomingBirthdayStaffGroup group
        ) noexcept
    {
        switch (group)
        {
        case UpcomingBirthdayStaffGroup::KoreanTeacher:
        case UpcomingBirthdayStaffGroup::NativeEnglishTeacher:
        case UpcomingBirthdayStaffGroup::GsTeam:
            return true;
        }

        return false;
    }

    [[nodiscard]] static std::optional<SysDays> toSysDays(
        const UpcomingBirthdayDate& date
        )
    {
        using namespace std::chrono;

        if (date.year < 1 || date.year > 9999
            || date.month < 1 || date.month > 12
            || date.day < 1 || date.day > 31)
        {
            return std::nullopt;
        }

        const year_month_day value{
            year{date.year},
            month{static_cast<unsigned>(date.month)},
            day{static_cast<unsigned>(date.day)}
        };
        if (!value.ok())
        {
            return std::nullopt;
        }

        return sys_days{value};
    }

    [[nodiscard]] static UpcomingBirthdayDate fromSysDays(
        const SysDays& value
        )
    {
        const std::chrono::year_month_day date{value};
        return {
            static_cast<int>(date.year()),
            static_cast<int>(static_cast<unsigned>(date.month())),
            static_cast<int>(static_cast<unsigned>(date.day()))
        };
    }

    [[nodiscard]] static std::optional<std::pair<int, int>> parseBirthday(
        std::string_view value
        )
    {
        while (!value.empty() && isAsciiWhitespace(value.front()))
        {
            value.remove_prefix(1);
        }
        while (!value.empty() && isAsciiWhitespace(value.back()))
        {
            value.remove_suffix(1);
        }

        if (value.size() != 5 || value[2] != '-')
        {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (index != 2 && (value[index] < '0' || value[index] > '9'))
            {
                return std::nullopt;
            }
        }

        const int monthValue = (value[0] - '0') * 10 + (value[1] - '0');
        const int dayValue = (value[3] - '0') * 10 + (value[4] - '0');
        const std::chrono::year_month_day date{
            std::chrono::year{2000},
            std::chrono::month{static_cast<unsigned>(monthValue)},
            std::chrono::day{static_cast<unsigned>(dayValue)}
        };
        if (!date.ok())
        {
            return std::nullopt;
        }

        return std::pair{monthValue, dayValue};
    }

    [[nodiscard]] static bool isAsciiWhitespace(const char value) noexcept
    {
        return value == ' ' || value == '\t' || value == '\n'
            || value == '\r' || value == '\f' || value == '\v';
    }

    [[nodiscard]] static bool isBlank(const std::u16string& value) noexcept
    {
        return std::all_of(
            value.begin(),
            value.end(),
            [](const char16_t character)
            {
                return isQtWhitespace(character);
            }
            );
    }

    [[nodiscard]] static std::optional<SysDays> nextOccurrence(
        const int monthValue,
        const int dayValue,
        const SysDays& rangeStart,
        const SysDays& rangeEnd
        )
    {
        using namespace std::chrono;

        const int firstYear = static_cast<int>(year_month_day{rangeStart}.year());
        const int lastYear = static_cast<int>(year_month_day{rangeEnd}.year());
        for (int yearValue = firstYear; yearValue <= lastYear; ++yearValue)
        {
            year_month_day occurrenceDate{
                year{yearValue},
                month{static_cast<unsigned>(monthValue)},
                day{static_cast<unsigned>(dayValue)}
            };
            if (!occurrenceDate.ok()
                && monthValue == 2
                && dayValue == 29)
            {
                occurrenceDate = year_month_day{year{yearValue} / February / day{28}};
            }

            if (!occurrenceDate.ok())
            {
                continue;
            }

            const sys_days occurrence{occurrenceDate};
            if (occurrence >= rangeStart && occurrence <= rangeEnd)
            {
                return occurrence;
            }
        }

        return std::nullopt;
    }
};

} // namespace ClassMngr::Next::Application

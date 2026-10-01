#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class ClassDayScheduleSource : std::uint8_t
{
    Regular,
    Intensive
};

enum class ClassDayVisibilityScope : std::uint8_t
{
    AllClasses,
    ActiveSchedule
};

// The feature boundary trims and case-folds these day keys before passing
// them to the policy. Empty keys represent unselected or whitespace-only days.
struct ClassDayFilterPolicy final
{
    std::vector<std::string> selectedDayKeys;
    ClassDayScheduleSource scheduleSource{ClassDayScheduleSource::Regular};
    ClassDayVisibilityScope visibilityScope{
        ClassDayVisibilityScope::AllClasses
    };
};

// These are normalized day keys, with one value per schedule entry. Their
// vector sizes also preserve whether the selected source contains any entries.
struct ClassDayScheduleDays final
{
    std::vector<std::string> regularDayKeys;
    std::vector<std::string> intensiveDayKeys;
};

[[nodiscard]] constexpr bool classDayFilterMatches(
    const ClassDayFilterPolicy& filter,
    const ClassDayScheduleDays& schedule
    ) noexcept
{
    const std::vector<std::string>& selectedScheduleDays =
        filter.scheduleSource == ClassDayScheduleSource::Intensive
        ? schedule.intensiveDayKeys
        : schedule.regularDayKeys;

    if (filter.visibilityScope == ClassDayVisibilityScope::ActiveSchedule
        && selectedScheduleDays.empty())
    {
        return false;
    }

    bool hasSelectedDay = false;
    for (const std::string& selectedDay : filter.selectedDayKeys)
    {
        if (selectedDay.empty())
        {
            continue;
        }

        hasSelectedDay = true;
        const bool selectedWeekend =
            selectedDay == "weekend" || selectedDay == "wkend";

        for (const std::string& scheduleDay : selectedScheduleDays)
        {
            if (selectedWeekend)
            {
                if (scheduleDay == "saturday" || scheduleDay == "sunday")
                {
                    return true;
                }
                continue;
            }

            if (selectedDay == scheduleDay)
            {
                return true;
            }
        }
    }

    return !hasSelectedDay;
}

} // namespace ClassMngr::Next::Application

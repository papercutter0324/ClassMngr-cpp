#pragma once

#include "classmngr/engine/academic_calendar.h"

#include <chrono>
#include <optional>
#include <string>

namespace classmngr::engine
{

enum class CalendarEventRepeatFrequency
{
    Daily,
    Weekly,
    Monthly
};

struct CalendarEvent
{
    int id = -1;
    std::string title;
    std::string eventType = "Other";
    std::string timeStatus = "Timed";
    std::string repeatSeriesId;
    bool allDay = false;
    CalendarDate startDate{};
    std::optional<std::chrono::minutes> startTime;
    CalendarDate endDate{};
    std::optional<std::chrono::minutes> endTime;
};

} // namespace classmngr::engine

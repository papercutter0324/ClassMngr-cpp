#pragma once

#include <compare>
#include <optional>

namespace ClassMngr::Next::Domain
{

enum class Weekday
{
    Monday = 0,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

class ScheduleTime final
{
public:
    [[nodiscard]] static std::optional<ScheduleTime> fromMinutes(
        int weekdayIndex,
        int startMinute,
        int endMinute
        )
    {
        if (weekdayIndex < static_cast<int>(Weekday::Monday)
            || weekdayIndex > static_cast<int>(Weekday::Sunday)
            || startMinute < 0
            || startMinute >= minutesPerDay
            || endMinute <= startMinute
            || endMinute >= minutesPerDay)
        {
            return std::nullopt;
        }

        return ScheduleTime(
            static_cast<Weekday>(weekdayIndex),
            startMinute,
            endMinute
            );
    }

    [[nodiscard]] Weekday weekday() const noexcept
    {
        return m_weekday;
    }

    [[nodiscard]] int weekdayIndex() const noexcept
    {
        return static_cast<int>(m_weekday);
    }

    [[nodiscard]] int startMinute() const noexcept
    {
        return m_startMinute;
    }

    [[nodiscard]] int endMinute() const noexcept
    {
        return m_endMinute;
    }

    [[nodiscard]] bool overlaps(const ScheduleTime& other) const noexcept
    {
        return m_weekday == other.m_weekday
            && m_startMinute < other.m_endMinute
            && other.m_startMinute < m_endMinute;
    }

    friend bool operator==(
        const ScheduleTime&,
        const ScheduleTime&
        ) = default;

    friend auto operator<=>(
        const ScheduleTime&,
        const ScheduleTime&
        ) = default;

private:
    static constexpr int minutesPerDay = 24 * 60;

    ScheduleTime(
        Weekday weekday,
        int startMinute,
        int endMinute
        ) noexcept
        : m_weekday(weekday)
        , m_startMinute(startMinute)
        , m_endMinute(endMinute)
    {
    }

    Weekday m_weekday;
    int m_startMinute;
    int m_endMinute;
};

} // namespace ClassMngr::Next::Domain

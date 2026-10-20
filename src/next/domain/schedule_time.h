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
    class StartMinute final
    {
    public:
        constexpr explicit StartMinute(const int rawValue) noexcept
            : m_rawValue(rawValue)
        {
        }

        [[nodiscard]] constexpr int rawValue() const noexcept
        {
            return m_rawValue;
        }

        friend constexpr bool operator==(
            const StartMinute&,
            const StartMinute&
            ) noexcept = default;

        friend constexpr auto operator<=>(
            const StartMinute&,
            const StartMinute&
            ) noexcept = default;

    private:
        int m_rawValue;
    };

    class EndMinute final
    {
    public:
        constexpr explicit EndMinute(const int rawValue) noexcept
            : m_rawValue(rawValue)
        {
        }

        [[nodiscard]] constexpr int rawValue() const noexcept
        {
            return m_rawValue;
        }

        friend constexpr bool operator==(
            const EndMinute&,
            const EndMinute&
            ) noexcept = default;

        friend constexpr auto operator<=>(
            const EndMinute&,
            const EndMinute&
            ) noexcept = default;

    private:
        int m_rawValue;
    };

    [[nodiscard]] static std::optional<ScheduleTime> fromMinutes(
        Weekday weekday,
        StartMinute startMinute,
        EndMinute endMinute
        )
    {
        if (weekday < Weekday::Monday
            || weekday > Weekday::Sunday
            || startMinute.rawValue() < 0
            || startMinute.rawValue() >= minutesPerDay
            || endMinute.rawValue() <= startMinute.rawValue()
            || endMinute.rawValue() >= minutesPerDay)
        {
            return std::nullopt;
        }

        return ScheduleTime(
            weekday,
            startMinute,
            endMinute
            );
    }

    [[nodiscard]] Weekday weekday() const noexcept
    {
        return m_weekday;
    }

    [[nodiscard]] StartMinute startMinute() const noexcept
    {
        return m_startMinute;
    }

    [[nodiscard]] EndMinute endMinute() const noexcept
    {
        return m_endMinute;
    }

    [[nodiscard]] bool overlaps(const ScheduleTime& other) const noexcept
    {
        return m_weekday == other.m_weekday
            && m_startMinute.rawValue() < other.m_endMinute.rawValue()
            && other.m_startMinute.rawValue() < m_endMinute.rawValue();
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
        StartMinute startMinute,
        EndMinute endMinute
        ) noexcept
        : m_weekday(weekday)
        , m_startMinute(startMinute)
        , m_endMinute(endMinute)
    {
    }

    Weekday m_weekday;
    StartMinute m_startMinute;
    EndMinute m_endMinute;
};

} // namespace ClassMngr::Next::Domain

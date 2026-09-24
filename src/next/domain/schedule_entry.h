#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/schedule_time.h"

#include <compare>
#include <utility>

namespace ClassMngr::Next::Domain
{

class ScheduleEntry final
{
public:
    ScheduleEntry(
        ClassId classId,
        ScheduleTime scheduleTime
        ) noexcept
        : m_classId(std::move(classId))
        , m_scheduleTime(std::move(scheduleTime))
    {
    }

    [[nodiscard]] const ClassId& classId() const noexcept
    {
        return m_classId;
    }

    [[nodiscard]] const ScheduleTime& scheduleTime() const noexcept
    {
        return m_scheduleTime;
    }

    friend bool operator==(
        const ScheduleEntry&,
        const ScheduleEntry&
        ) = default;

    friend auto operator<=>(
        const ScheduleEntry&,
        const ScheduleEntry&
        ) = default;

private:
    ClassId m_classId;
    ScheduleTime m_scheduleTime;
};

} // namespace ClassMngr::Next::Domain

#pragma once

#include "next/domain/operation_result.h"

#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// An exact owning identity for a slot state's raw weekday and start-time
// values. These UTF-16 strings are not parsed or normalized on the read path.
class ScheduleSlotStateReadIdentity final
{
public:
    ScheduleSlotStateReadIdentity() = default;

    explicit ScheduleSlotStateReadIdentity(
        std::u16string day,
        std::u16string startTime
        )
        : m_day(std::move(day))
        , m_startTime(std::move(startTime))
    {
    }

    [[nodiscard]] const std::u16string& day() const noexcept
    {
        return m_day;
    }

    [[nodiscard]] const std::u16string& startTime() const noexcept
    {
        return m_startTime;
    }

    friend bool operator==(
        const ScheduleSlotStateReadIdentity&,
        const ScheduleSlotStateReadIdentity&
        ) = default;

private:
    std::u16string m_day;
    std::u16string m_startTime;
};

struct ScheduleSlotStateReadQuery final
{
    friend bool operator==(
        const ScheduleSlotStateReadQuery&,
        const ScheduleSlotStateReadQuery&
        ) = default;
};

struct ScheduleSlotStateReadRow final
{
    ScheduleSlotStateReadIdentity identity;
    std::u16string state;

    friend bool operator==(
        const ScheduleSlotStateReadRow&,
        const ScheduleSlotStateReadRow&
        ) = default;
};

struct ScheduleSlotStateReadSnapshot final
{
    std::vector<ScheduleSlotStateReadRow> rows;

    friend bool operator==(
        const ScheduleSlotStateReadSnapshot&,
        const ScheduleSlotStateReadSnapshot&
        ) = default;
};

using ScheduleSlotStateReadResult =
    Domain::Result<ScheduleSlotStateReadSnapshot>;

class ScheduleSlotStateReadPort
{
public:
    virtual ~ScheduleSlotStateReadPort() = default;

    [[nodiscard]] virtual ScheduleSlotStateReadResult readSlotStates(
        const ScheduleSlotStateReadQuery& query
        ) const = 0;
};

class ScheduleSlotStateReadQueryHandler final
{
public:
    [[nodiscard]] static ScheduleSlotStateReadResult execute(
        const ScheduleSlotStateReadQuery& query,
        const ScheduleSlotStateReadPort& port
        )
    {
        return port.readSlotStates(query);
    }
};

} // namespace ClassMngr::Next::Application

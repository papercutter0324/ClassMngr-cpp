#pragma once

#include "next/domain/operation_result.h"

#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ScheduleSlotStateReadQuery final
{
    friend bool operator==(
        const ScheduleSlotStateReadQuery&,
        const ScheduleSlotStateReadQuery&
        ) = default;
};

struct ScheduleSlotStateReadRow final
{
    std::u16string day;
    std::u16string startTime;
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

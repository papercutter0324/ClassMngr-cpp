#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

enum class ScheduleWeekday
{
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

enum class ScheduleSlotState
{
    Empty,
    Essay,
    Lunch
};

// This command describes the selected state for one globally keyed schedule
// slot. The view model owns the state transition and supplies the default.
struct ScheduleSlotStateSaveRequest final
{
    ScheduleWeekday weekday = ScheduleWeekday::Monday;
    int startMinute = 0;
    ScheduleSlotState selectedState = ScheduleSlotState::Empty;
    ScheduleSlotState defaultState = ScheduleSlotState::Empty;

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const ScheduleSlotStateSaveRequest&,
        const ScheduleSlotStateSaveRequest&
        ) = default;
};

using ScheduleSlotStateSaveResult = Domain::Result<void>;

class ScheduleSlotStateSavePort
{
public:
    virtual ~ScheduleSlotStateSavePort() = default;

    [[nodiscard]] virtual ScheduleSlotStateSaveResult saveSlotState(
        const ScheduleSlotStateSaveRequest& request
        ) const = 0;
};

[[nodiscard]] inline Domain::Result<void>
validateScheduleSlotStateSaveRequest(
    const ScheduleSlotStateSaveRequest& request
    )
{
    const bool validWeekday =
        request.weekday >= ScheduleWeekday::Monday
        && request.weekday <= ScheduleWeekday::Sunday;
    if (!validWeekday)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Schedule weekday is invalid.",
            .recoverable = false
        });
    }

    if (request.startMinute < 0 || request.startMinute >= 24 * 60)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Schedule slot start minute must be within the day.",
            .recoverable = false
        });
    }

    const auto isValidState = [](const ScheduleSlotState state)
    {
        return state == ScheduleSlotState::Empty
            || state == ScheduleSlotState::Essay
            || state == ScheduleSlotState::Lunch;
    };
    if (!isValidState(request.selectedState)
        || !isValidState(request.defaultState))
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Schedule slot state is invalid.",
            .recoverable = false
        });
    }

    return Domain::Result<void>::success();
}

inline Domain::Result<void> ScheduleSlotStateSaveRequest::validate() const
{
    return validateScheduleSlotStateSaveRequest(*this);
}

} // namespace ClassMngr::Next::Application

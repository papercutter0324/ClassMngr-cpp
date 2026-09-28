#pragma once

#include "next/application/schedule_slot_state_save.h"

namespace ClassMngr::Next::Application
{

class ScheduleSlotStateSaveUseCase final
{
public:
    [[nodiscard]] static ScheduleSlotStateSaveResult execute(
        const ScheduleSlotStateSaveRequest& request,
        const ScheduleSlotStateSavePort& port
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return ScheduleSlotStateSaveResult::failure(
                validation.error()
                );
        }

        return port.saveSlotState(request);
    }
};

} // namespace ClassMngr::Next::Application

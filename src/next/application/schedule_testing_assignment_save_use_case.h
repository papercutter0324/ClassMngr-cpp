#pragma once

#include "next/application/schedule_testing_assignment_save.h"

namespace ClassMngr::Next::Application
{

class ScheduleTestingAssignmentSaveUseCase final
{
public:
    [[nodiscard]] static ScheduleTestingAssignmentSaveResult execute(
        const ScheduleTestingAssignmentSaveRequest& request,
        const ScheduleTestingAssignmentSavePort& port
        )
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return ScheduleTestingAssignmentSaveResult::failure(
                validation.error()
                );
        }

        return port.saveTestingAssignment(request);
    }
};

} // namespace ClassMngr::Next::Application

#pragma once

#include "next/application/schedule_testing_assignment_save.h"

#include <utility>

namespace ClassMngr::Next::Application
{

class ScheduleTestingAssignmentSaveUseCase final
{
public:
    template <typename WeekdayCaseInsensitiveEquals>
    [[nodiscard]] static ScheduleTestingAssignmentSaveResult execute(
        const ScheduleTestingAssignmentSaveRequest& request,
        const ScheduleTestingAssignmentSavePort& port,
        WeekdayCaseInsensitiveEquals&& weekdayCaseInsensitiveEquals
        )
    {
        const Domain::Result<void> validation = request.validate(
            std::forward<WeekdayCaseInsensitiveEquals>(
                weekdayCaseInsensitiveEquals
                )
            );
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

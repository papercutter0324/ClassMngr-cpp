#pragma once

#include "next/application/schedule_view_projection.h"
#include "next/domain/operation_result.h"

#include <utility>

namespace ClassMngr::Next::Application
{

using MyWorkspaceScheduleReadResult = Domain::Result<ScheduleViewProjection>;

class MyWorkspaceScheduleReadPort
{
public:
    virtual ~MyWorkspaceScheduleReadPort() = default;

    [[nodiscard]] virtual MyWorkspaceScheduleReadResult readSchedule() const = 0;
};

// The page's read-only schedule use case accepts only the bounded visible
// projection. Persistence and schedule formatting stay behind the port.
class MyWorkspaceScheduleReadUseCase final
{
public:
    [[nodiscard]] static MyWorkspaceScheduleReadResult execute(
        const MyWorkspaceScheduleReadPort& port
        )
    {
        MyWorkspaceScheduleReadResult loaded = port.readSchedule();
        if (!loaded)
        {
            return MyWorkspaceScheduleReadResult::failure(loaded.error());
        }
        if (loaded.value().rowCount() > kScheduleViewMaxRows
            || loaded.value().cellCount() > kScheduleViewMaxCells)
        {
            return MyWorkspaceScheduleReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "The workspace schedule exceeds its bounded view capacity.",
                .recoverable = false
            });
        }
        return MyWorkspaceScheduleReadResult::success(std::move(loaded.value()));
    }
};

} // namespace ClassMngr::Next::Application

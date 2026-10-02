#pragma once

#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_apply_validation.h"

#include <utility>

namespace ClassMngr::Next::Application
{

class ScheduleImportApplyUseCase final
{
public:
    [[nodiscard]] static ScheduleImportApplyResult execute(
        const ScheduleImportApplyRequest& request,
        const ScheduleImportApplyWritePort& port
        )
    {
        if (auto failure = validateScheduleImportApplyRequest(request))
        {
            return std::unexpected(std::move(*failure));
        }
        return port.applyScheduleImport(request);
    }
};

} // namespace ClassMngr::Next::Application

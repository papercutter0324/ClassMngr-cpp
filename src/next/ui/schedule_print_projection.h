#pragma once

#include "features/schedule/services/schedule_print_document.h"

namespace ClassMngr::Next::Ui
{

class SchedulePageModel;

// Copies only the visible schedule values needed by the shared renderer.
[[nodiscard]] SchedulePrintService::Document schedulePrintDocument(
    const SchedulePageModel& model
    );

} // namespace ClassMngr::Next::Ui

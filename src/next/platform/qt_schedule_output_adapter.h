#pragma once

#include "next/ui/schedule_output_request.h"

class QWidget;

namespace ClassMngr::Next::Platform
{

// Qt print/PDF objects are created inside each call and released before it
// returns. The adapter keeps no workspace or output state between operations.
[[nodiscard]] Ui::ScheduleOutputResult performScheduleOutput(
    Ui::ScheduleOutputRequest request,
    QWidget* parent
    );

} // namespace ClassMngr::Next::Platform

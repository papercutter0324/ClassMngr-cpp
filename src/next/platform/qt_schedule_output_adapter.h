#pragma once

#include "next/platform/pdf_print_operation.h"
#include "next/ui/schedule_output_request.h"

class QWidget;

namespace ClassMngr::Next::Platform
{

// Qt print/PDF objects are created inside each call and released before it
// returns. The adapter keeps no workspace or output state between operations.
[[nodiscard]] Ui::ScheduleOutputResult performScheduleOutput(
    Ui::ScheduleOutputRequest request,
    QWidget* parent,
    PdfPrintOperationFactory printOperationFactory =
        makeQtPdfPrintOperation
    );

} // namespace ClassMngr::Next::Platform

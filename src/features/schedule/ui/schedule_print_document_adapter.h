#pragma once

#include "features/schedule/services/schedule_print_document.h"
#include "features/schedule/ui/schedule_view_model.h"

[[nodiscard]] SchedulePrintService::Document
schedulePrintDocumentFromLegacyModel(const ScheduleViewModel& model);

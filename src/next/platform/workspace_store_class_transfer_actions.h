#pragma once

#include "next/application/class_transfer_apply.h"
#include "next/application/class_transfer_export_query.h"
#include "next/application/class_transfer_preview_query.h"
#include "next/application/classes_list_read_query.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::ClassesListReadResult
readClassTransferClassesAtPath(const QString& databasePath);

[[nodiscard]] Application::ClassTransferExportResult
exportClassTransferAtPath(
    const QString& databasePath,
    const Application::ClassTransferExportRequest& request
    );

[[nodiscard]] Application::ClassTransferPreviewQueryResult
previewClassTransferAtPath(
    const QString& databasePath,
    const Application::ClassTransferPreviewRequest& request
    );

[[nodiscard]] Application::ClassTransferApplyResult
applyClassTransferAtPath(
    const QString& databasePath,
    Application::ClassTransferApplyCommand command
    );

} // namespace ClassMngr::Next::Platform

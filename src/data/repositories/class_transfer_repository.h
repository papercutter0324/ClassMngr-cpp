#pragma once

#include "core/result.h"
#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_apply_request.h"

#include <QList>
#include <QSqlDatabase>

class ClassTransferRepository
{
public:
    explicit ClassTransferRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] Result<ClassTransferPackage> buildPackage(
        const QList<int>& classIds
        );

    [[nodiscard]] Result<ClassImportPreview> previewImport(
        const ClassTransferPackage& package
        );

    [[nodiscard]] Result<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassImportPlan& plan
        );

    [[nodiscard]] Result<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassMngr::Next::Application::ClassTransferApplyRequest& request
        );

private:
    QSqlDatabase& m_database;
};

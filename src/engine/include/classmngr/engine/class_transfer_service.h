#pragma once

#include "classmngr/engine/class_transfer.h"
#include "classmngr/engine/result.h"

#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

// Portable class package workflows shared by the retained Qt and native
// presentation stacks.  JSON and file-system codecs translate at this edge;
// package matching, schedule preflight, and transactional persistence live in
// the engine.
class ClassTransferService final
{
public:
    explicit ClassTransferService(
        SqliteDatabase& database
        );

    [[nodiscard]] Result<ClassTransferPackage> buildPackage(
        const std::vector<int>& classIds
        );

    [[nodiscard]] Result<ClassImportPreview> previewImport(
        const ClassTransferPackage& package
        );

    [[nodiscard]] Result<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassImportPlan& plan
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine

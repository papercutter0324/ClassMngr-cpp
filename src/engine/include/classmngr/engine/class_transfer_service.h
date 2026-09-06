#pragma once

#include "classmngr/engine/class_transfer.h"
#include "classmngr/engine/platform_services.h"
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

    ClassTransferService(
        SqliteDatabase& database,
        const Clock& clock
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
    SystemClock m_systemClock;
    const Clock* m_clock = nullptr;
};

} // namespace classmngr::engine

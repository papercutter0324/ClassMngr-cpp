#pragma once

#include "classmngr/engine/platform_services.h"
#include "classmngr/engine/result.h"

#include <string>
#include <vector>

namespace classmngr::engine::ZipArchiveWriter
{

struct Entry
{
    std::string sourcePath;
    std::string archiveName;
};

// Paths and archive names are UTF-8.  Error.message contains a stable
// machine-readable token for presentation adapters to localize.
[[nodiscard]] Status writeArchive(
    const std::string& archivePath,
    const std::vector<Entry>& entries
    );

// Uses clock values for temporary archive names and fallback ZIP timestamps.
// The overload above preserves the system-clock behavior for existing callers.
[[nodiscard]] Status writeArchive(
    const std::string& archivePath,
    const std::vector<Entry>& entries,
    const Clock& clock
    );

} // namespace classmngr::engine::ZipArchiveWriter

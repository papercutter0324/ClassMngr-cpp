#pragma once

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

} // namespace classmngr::engine::ZipArchiveWriter

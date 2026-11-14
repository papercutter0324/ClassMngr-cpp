#pragma once

#include "core/archive_read_port.h"

#include <memory>

// Creates the Qt/zlib ZIP reader used by application composition roots.
// Shared workbook readers and parsers depend only on ArchiveReadPort.
[[nodiscard]] std::unique_ptr<ArchiveReadPort> makeZipArchiveReadPort();

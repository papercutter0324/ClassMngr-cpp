#pragma once

#include "core/archive_write_port.h"

#include <memory>

[[nodiscard]] std::shared_ptr<ArchiveWritePort> makeZipArchiveWritePort();

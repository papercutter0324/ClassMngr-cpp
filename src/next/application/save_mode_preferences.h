#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries the typed save mode. Persistence and
// legacy integer conversion belong to the platform adapter.
enum class SaveMode
{
    Automatic = 0,
    Manual = 1
};

class SaveModePreferencesPort
{
public:
    virtual ~SaveModePreferencesPort() = default;

    [[nodiscard]] virtual SaveMode read() const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        SaveMode mode
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

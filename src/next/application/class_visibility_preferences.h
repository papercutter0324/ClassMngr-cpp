#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed class visibility scope.
// Persistence and legacy value conversion belong to the outer adapter.
enum class ClassVisibilityScope
{
    ActiveSchedule,
    AllClasses
};

using ClassVisibilityPreferencesSaveResult = Domain::Result<void>;
using ClassVisibilityPreferencesSaveError = Domain::OperationError;

class ClassVisibilityPreferencesPort
{
public:
    virtual ~ClassVisibilityPreferencesPort() = default;

    [[nodiscard]] virtual ClassVisibilityScope load() const = 0;

    [[nodiscard]] virtual ClassVisibilityPreferencesSaveResult save(
        ClassVisibilityScope scope
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed class-selection reset
// policy. Persistence and legacy value conversion belong to the outer
// adapter.
enum class ClassSelectionResetPolicy
{
    OnApplicationClose,
    OnPageLeave
};

using ClassSelectionResetPolicyLoadResult =
    Domain::Result<ClassSelectionResetPolicy>;
using ClassSelectionResetPolicySaveResult = Domain::Result<void>;
using ClassSelectionResetPolicySaveError = Domain::OperationError;

class ClassSelectionResetPolicyPort
{
public:
    virtual ~ClassSelectionResetPolicyPort() = default;

    [[nodiscard]] virtual ClassSelectionResetPolicyLoadResult load()
        const = 0;

    [[nodiscard]] virtual ClassSelectionResetPolicySaveResult save(
        ClassSelectionResetPolicy policy
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

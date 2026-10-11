#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed class day-filter reset
// policy. Persistence and legacy value conversion belong to the outer
// adapter.
enum class ClassDayFilterResetPolicy
{
    OnApplicationClose,
    OnPageLeave
};

using ClassDayFilterResetPolicySaveResult = Domain::Result<void>;
using ClassDayFilterResetPolicySaveError = Domain::OperationError;

class ClassDayFilterResetPolicyPort
{
public:
    virtual ~ClassDayFilterResetPolicyPort() = default;

    [[nodiscard]] virtual ClassDayFilterResetPolicy load() const = 0;

    [[nodiscard]] virtual ClassDayFilterResetPolicySaveResult save(
        ClassDayFilterResetPolicy policy
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

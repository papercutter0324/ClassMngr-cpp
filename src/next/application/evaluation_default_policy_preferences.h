#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries only the two supported evaluation-default
// policies. Persistence and legacy QVariant conversion belong to the outer
// adapter.
enum class EvaluationDefaultPolicy
{
    All,
    CurrentOrPreviousTerm
};

using EvaluationDefaultPolicyPreferencesSaveResult = Domain::Result<void>;
using EvaluationDefaultPolicyPreferencesSaveError = Domain::OperationError;

class EvaluationDefaultPolicyPreferencesPort
{
public:
    virtual ~EvaluationDefaultPolicyPreferencesPort() = default;

    [[nodiscard]] virtual EvaluationDefaultPolicy load() const = 0;

    [[nodiscard]] virtual EvaluationDefaultPolicyPreferencesSaveResult save(
        EvaluationDefaultPolicy policy
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

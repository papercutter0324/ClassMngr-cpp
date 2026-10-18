#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using MiddleSchoolAnalyticsPreferencesSaveResult = Domain::Result<void>;
using MiddleSchoolAnalyticsPreferencesReadResult = Domain::Result<bool>;
using MiddleSchoolAnalyticsPreferencesSaveError = Domain::OperationError;

// The application boundary carries only the typed middle-school analytics
// visibility preference. Persistence and legacy QVariant conversion belong to
// the outer adapter.
class MiddleSchoolAnalyticsPreferencesPort
{
public:
    virtual ~MiddleSchoolAnalyticsPreferencesPort() = default;

    [[nodiscard]] virtual MiddleSchoolAnalyticsPreferencesReadResult load()
        const = 0;

    [[nodiscard]] virtual MiddleSchoolAnalyticsPreferencesSaveResult save(
        bool show
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

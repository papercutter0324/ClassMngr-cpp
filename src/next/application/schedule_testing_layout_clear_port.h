#pragma once

#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

using ScheduleTestingLayoutClearResult = Domain::Result<void>;

// A typed command boundary for clearing all rows in the active testing layout.
// Availability stays separate so the Preferences action can keep its existing
// warning-before-confirmation behavior.
class ScheduleTestingLayoutClearPort
{
public:
    virtual ~ScheduleTestingLayoutClearPort() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;

    [[nodiscard]] virtual ScheduleTestingLayoutClearResult
        clearTestingLayout() const = 0;
};

} // namespace ClassMngr::Next::Application

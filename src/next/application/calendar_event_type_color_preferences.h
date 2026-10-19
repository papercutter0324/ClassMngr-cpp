#pragma once

#include "next/domain/calendar_event_timing.h"
#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries an already-normalized event type and an
// opaque color string. UI code owns event-type normalization and color
// conversion; persistence belongs to the platform adapter.
class CalendarEventTypeColorPreferencesPort
{
public:
    using SaveResult = Domain::Result<void>;

    virtual ~CalendarEventTypeColorPreferencesPort() = default;

    [[nodiscard]] virtual std::string read(
        Domain::CalendarEventType eventType
        ) const = 0;

    [[nodiscard]] virtual SaveResult write(
        Domain::CalendarEventType eventType,
        const std::string& colorHexRgb
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

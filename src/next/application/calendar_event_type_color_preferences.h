#pragma once

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries an already-normalized event type and an
// opaque color string. UI code owns event-type normalization and color
// conversion; persistence belongs to the platform adapter.
class CalendarEventTypeColorPreferencesPort
{
public:
    virtual ~CalendarEventTypeColorPreferencesPort() = default;

    [[nodiscard]] virtual std::string read(
        const std::string& normalizedEventType
        ) const = 0;

    virtual void write(
        const std::string& normalizedEventType,
        const std::string& colorHexRgb
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

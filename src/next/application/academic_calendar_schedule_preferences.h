#pragma once

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the academic-calendar schedule as an
// opaque UTF-8 JSON payload. JSON parsing and schema ownership remain with
// the calendar feature.
class AcademicCalendarSchedulePreferencesPort
{
public:
    virtual ~AcademicCalendarSchedulePreferencesPort() = default;

    [[nodiscard]] virtual std::string read() const = 0;

    virtual void write(const std::string& payload) const = 0;
};

} // namespace ClassMngr::Next::Application

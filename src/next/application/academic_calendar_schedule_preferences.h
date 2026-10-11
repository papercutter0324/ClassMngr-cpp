#pragma once

#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the academic-calendar schedule as an
// opaque UTF-8 JSON payload. JSON parsing and schema ownership remain with
// the calendar feature.
using AcademicCalendarSchedulePreferencesSaveResult = Domain::Result<void>;

class AcademicCalendarSchedulePreferencesPort
{
public:
    virtual ~AcademicCalendarSchedulePreferencesPort() = default;

    [[nodiscard]] virtual std::string read() const = 0;

    [[nodiscard]] virtual AcademicCalendarSchedulePreferencesSaveResult write(
        const std::string& payload
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

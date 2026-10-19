#pragma once

#include "next/domain/operation_result.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// The application boundary carries the academic-calendar schedule as an
// opaque UTF-8 JSON payload. This value preserves supplied text unchanged;
// JSON parsing and schema ownership remain with the calendar feature.
class AcademicCalendarScheduleJsonText final
{
public:
    AcademicCalendarScheduleJsonText() = default;

    explicit AcademicCalendarScheduleJsonText(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const& noexcept
    {
        return m_value;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    friend bool operator==(
        const AcademicCalendarScheduleJsonText&,
        const AcademicCalendarScheduleJsonText&
        ) = default;

private:
    std::string m_value;
};

using AcademicCalendarSchedulePreferencesReadResult =
    Domain::Result<AcademicCalendarScheduleJsonText>;
using AcademicCalendarSchedulePreferencesSaveResult = Domain::Result<void>;

class AcademicCalendarSchedulePreferencesPort
{
public:
    virtual ~AcademicCalendarSchedulePreferencesPort() = default;

    [[nodiscard]] virtual AcademicCalendarSchedulePreferencesReadResult read()
        const = 0;

    [[nodiscard]] virtual AcademicCalendarSchedulePreferencesSaveResult write(
        const AcademicCalendarScheduleJsonText& payload
        ) const = 0;
};

} // namespace ClassMngr::Next::Application

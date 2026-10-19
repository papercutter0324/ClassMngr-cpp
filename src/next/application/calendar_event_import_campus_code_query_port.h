#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// An owned UTF-8 campus code from the calendar-import platform boundary.
// The bytes are kept verbatim; normalization belongs to the adapter.
class CalendarEventImportCampusCode final
{
public:
    explicit CalendarEventImportCampusCode(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const CalendarEventImportCampusCode&,
        const CalendarEventImportCampusCode&
        ) = default;

private:
    std::string m_value;
};

// Supplies campus codes used while parsing calendar-import workbooks. The
// returned UTF-8 values preserve the legacy directory ordering and filtering
// rules without exposing the Qt-backed campus repository to Application.
class CalendarEventImportCampusCodeQueryPort
{
public:
    virtual ~CalendarEventImportCampusCodeQueryPort() = default;

    [[nodiscard]] virtual std::vector<CalendarEventImportCampusCode>
    loadCampusCodes() const = 0;
};

} // namespace ClassMngr::Next::Application

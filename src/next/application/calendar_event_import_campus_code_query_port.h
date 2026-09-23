#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// Supplies campus codes used while parsing calendar-import workbooks. The
// returned UTF-8 values preserve the legacy directory ordering and filtering
// rules without exposing the Qt-backed campus repository to Application.
class CalendarEventImportCampusCodeQueryPort
{
public:
    virtual ~CalendarEventImportCampusCodeQueryPort() = default;

    [[nodiscard]] virtual std::vector<std::string> loadCampusCodes() const = 0;
};

} // namespace ClassMngr::Next::Application

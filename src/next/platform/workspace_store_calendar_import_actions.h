#pragma once

#include "next/application/calendar_event_import_use_case.h"

#include <QString>

#include <functional>

namespace ClassMngr::Next::Platform
{

// Opens exactly the supplied workspace path and applies one typed Calendar
// import through that store. The callback is checked before queries and again
// immediately before the repository's batch transaction begins.
[[nodiscard]] Domain::Result<Application::CalendarEventImportUseCaseResult>
applyCalendarEventImportAtPath(
    const QString& databasePath,
    const Application::CalendarEventImportUseCaseRequest& request,
    std::function<bool()> mayContinue = {}
    );

} // namespace ClassMngr::Next::Platform

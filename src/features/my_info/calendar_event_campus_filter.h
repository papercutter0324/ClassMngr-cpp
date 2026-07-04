#pragma once

#include "domain/models/calendar_event.h"

#include <QStringList>

namespace CalendarEventCampusFilter
{
[[nodiscard]] bool eventMatchesCampus(
    const CalendarEvent& event,
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses
    );
}

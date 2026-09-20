#pragma once

#include "domain/models/calendar_event.h"
#include "next/application/calendar_event_projection.h"

#include <QStringList>

namespace CalendarEventCampusFilter
{
[[nodiscard]] bool eventMatchesCampus(
    const QString& title,
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses
    );

[[nodiscard]] bool eventMatchesCampus(
    const ClassMngr::Next::Application::CalendarEventSummary& event,
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses
    );

[[nodiscard]] bool eventMatchesCampus(
    const CalendarEvent& event,
    const QStringList& currentCampusCodes,
    const QStringList& allCampusCodes,
    bool showAllCampuses
    );
}

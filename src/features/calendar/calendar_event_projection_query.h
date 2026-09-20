#pragma once

#include "next/application/calendar_event_projection.h"

#include <QDate>
#include <QString>

class CalendarEventProjectionQuery final
{
public:
    using Projection =
        ClassMngr::Next::Application::CalendarEventProjection;
    using ProjectionResult =
        ClassMngr::Next::Domain::Result<Projection>;
    using NextEventDateResult =
        ClassMngr::Next::Domain::Result<QDate>;

    [[nodiscard]] static ProjectionResult loadRange(
        QString databasePath,
        QDate startDate,
        QDate endDate
        );

    [[nodiscard]] static NextEventDateResult findNextEventDate(
        QString databasePath,
        QDate afterDate
        );
};

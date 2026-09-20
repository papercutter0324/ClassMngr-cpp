#pragma once

#include "next/application/calendar_event_query_port.h"

#include <QDate>
#include <QString>

class CalendarEventProjectionQuery final
    : public ClassMngr::Next::Application::CalendarEventQueryPort
{
public:
    using Projection =
        ClassMngr::Next::Application::CalendarEventProjection;
    using ProjectionResult =
        ClassMngr::Next::Domain::Result<Projection>;
    using NextEventDateResult =
        ClassMngr::Next::Domain::Result<QDate>;
    using QueryDate =
        ClassMngr::Next::Application::CalendarEventDate;
    using QueryDateResult =
        ClassMngr::Next::Application::CalendarEventDateResult;

    ~CalendarEventProjectionQuery() override = default;

    CalendarEventProjectionQuery() = default;
    CalendarEventProjectionQuery(const CalendarEventProjectionQuery&) = delete;
    CalendarEventProjectionQuery& operator=(
        const CalendarEventProjectionQuery&
        ) = delete;
    CalendarEventProjectionQuery(CalendarEventProjectionQuery&&) = delete;
    CalendarEventProjectionQuery& operator=(
        CalendarEventProjectionQuery&&
        ) = delete;

    [[nodiscard]] ProjectionResult loadRange(
        const ClassMngr::Next::Application::CalendarEventRangeRequest& request
        ) override;

    [[nodiscard]] QueryDateResult findNextEventDate(
        const ClassMngr::Next::Application::CalendarEventNextEventRequest& request
        ) override;

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

class CalendarEventProjectionQueryFactory final
    : public ClassMngr::Next::Application::CalendarEventQueryPortFactory
{
public:
    [[nodiscard]] std::unique_ptr<
        ClassMngr::Next::Application::CalendarEventQueryPort
        > create() const override;
};

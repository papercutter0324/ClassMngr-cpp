#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/sub_prep_calendar_event_intervals_query.h"

#include <QByteArray>
#include <QDate>
#include <QString>

#include <exception>
#include <functional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Reads a narrow calendar interval snapshot through the legacy feature
// service and copies only the three values required by Sub Prep.
class ApplicationServicesSubPrepCalendarEventIntervalsPort final
    : public Application::SubPrepCalendarEventIntervalsReadPort
{
public:
    using IntervalRangeReader = std::function<
        ::Result<QList<CalendarEventDateInterval>>(
            const QDate&,
            const QDate&
            )>;

    explicit ApplicationServicesSubPrepCalendarEventIntervalsPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesSubPrepCalendarEventIntervalsPort(
        IntervalRangeReader intervalRangeReader
        )
        : m_intervalRangeReader(std::move(intervalRangeReader))
    {
    }

    ApplicationServicesSubPrepCalendarEventIntervalsPort(
        const ApplicationServicesSubPrepCalendarEventIntervalsPort&
        ) = delete;
    ApplicationServicesSubPrepCalendarEventIntervalsPort& operator=(
        const ApplicationServicesSubPrepCalendarEventIntervalsPort&
        ) = delete;
    ApplicationServicesSubPrepCalendarEventIntervalsPort(
        ApplicationServicesSubPrepCalendarEventIntervalsPort&&
        ) = delete;
    ApplicationServicesSubPrepCalendarEventIntervalsPort& operator=(
        ApplicationServicesSubPrepCalendarEventIntervalsPort&&
        ) = delete;

    [[nodiscard]] Application::SubPrepCalendarEventIntervalsReadResult
    loadIntervals(
        const Application::SubPrepCalendarEventIntervalsReadRequest& request
        ) override
    {
        const auto startDate = parseDate(request.startDate.value());
        const auto endDate = parseDate(request.endDate.value());
        if (!startDate.isValid()
            || !endDate.isValid()
            || endDate < startDate)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Sub Prep calendar interval range must contain valid ordered dates."
                );
        }

        try
        {
            const CalendarService* service = nullptr;
            if (!m_intervalRangeReader)
            {
                service = m_services
                    ? m_services->calendarService()
                    : nullptr;
                if (!service || !service->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The calendar service is unavailable."
                        );
                }
            }

            const ::Result<QList<CalendarEventDateInterval>> loaded =
                m_intervalRangeReader
                    ? m_intervalRangeReader(startDate, endDate)
                    : service->eventDateIntervalsInRange(startDate, endDate);
            if (!loaded)
            {
                if (service && !service->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The calendar service is unavailable."
                        );
                }

                const QByteArray errorBytes = loaded.error().toUtf8();
                return failure(
                    Domain::ErrorCode::Technical,
                    errorBytes.isEmpty()
                        ? "Sub Prep calendar intervals could not be loaded."
                        : "Sub Prep calendar intervals could not be loaded: "
                            + errorBytes.toStdString()
                    );
            }

            Application::SubPrepCalendarEventIntervals intervals;
            for (const CalendarEventDateInterval& source : loaded.value())
            {
                const QString eventType = source.eventType.trimmed();
                if (eventType != QStringLiteral("Vacation")
                    && eventType != QStringLiteral("Holiday"))
                {
                    continue;
                }

                const QByteArray eventTypeBytes = eventType.toUtf8();
                intervals.push_back({
                    .eventType = eventTypeBytes.toStdString(),
                    .startDate = Application::CalendarEventDate(
                        source.startDate.toString(Qt::ISODate).toStdString()
                        ),
                    .endDate = Application::CalendarEventDate(
                        source.endDate.toString(Qt::ISODate).toStdString()
                        )
                });
            }

            return Application::SubPrepCalendarEventIntervalsReadResult::success(
                std::move(intervals)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Sub Prep calendar intervals could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Sub Prep calendar intervals could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static QDate parseDate(const std::string& value)
    {
        const QString text = QString::fromStdString(value);
        const QDate parsed = QDate::fromString(text, Qt::ISODate);
        return parsed.isValid() && parsed.toString(Qt::ISODate) == text
            ? parsed
            : QDate();
    }

    [[nodiscard]] static Application::SubPrepCalendarEventIntervalsReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::SubPrepCalendarEventIntervalsReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices* m_services = nullptr;
    IntervalRangeReader m_intervalRangeReader;
};

} // namespace ClassMngr::Next::Platform

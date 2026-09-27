#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_event_series_edit_port.h"
#include "next/application/calendar_event_series_edit_plan.h"

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QTime>
#include <QString>

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for editing a repeat-series suffix. The legacy service
// owns recurrence selection and persistence; the Application planner owns
// the Qt-free occurrence transformation and this adapter applies its updates.
class ApplicationServicesCalendarEventSeriesEditPort final
    : public Application::CalendarEventSeriesEditPort
{
public:
    explicit ApplicationServicesCalendarEventSeriesEditPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesCalendarEventSeriesEditPort(
        const ApplicationServicesCalendarEventSeriesEditPort&
        ) = delete;
    ApplicationServicesCalendarEventSeriesEditPort& operator=(
        const ApplicationServicesCalendarEventSeriesEditPort&
        ) = delete;
    ApplicationServicesCalendarEventSeriesEditPort(
        ApplicationServicesCalendarEventSeriesEditPort&&
        ) = delete;
    ApplicationServicesCalendarEventSeriesEditPort& operator=(
        ApplicationServicesCalendarEventSeriesEditPort&&
        ) = delete;

    [[nodiscard]] Application::CalendarEventSeriesEditResult
    editRepeatSeriesFromDate(
        const Application::CalendarEventSeriesEditRequest& request
        ) override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return failure(
                validation.error().code,
                validation.error().message
                );
        }

        try
        {
            const CalendarService* service = m_services.calendarService();
            if (!service || !service->isAvailable())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The calendar service is unavailable."
                    );
            }

            const QDate startDate = legacyDate(request.startDate);
            const QDate editedStartDate = legacyDate(request.editedStartDate);
            const QDate editedEndDate = legacyDate(request.editedEndDate);
            if (!startDate.isValid()
                || !editedStartDate.isValid()
                || !editedEndDate.isValid()
                || editedEndDate < editedStartDate)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Calendar repeat-series edit dates must be valid and ordered."
                    );
            }

            const QString repeatSeriesId = legacyText(
                request.repeatSeriesId
                ).trimmed();
            const ::Result<QList<CalendarEvent>> seriesEvents =
                service->repeatSeriesFromDate(
                    repeatSeriesId,
                    startDate
                    );
            if (!seriesEvents)
            {
                if (!service->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The calendar service is unavailable."
                        );
                }

                const QByteArray errorBytes = seriesEvents.error().toUtf8();
                return failure(
                    Domain::ErrorCode::Technical,
                    errorBytes.isEmpty()
                        ? "Calendar repeat series could not be loaded."
                        : errorBytes.toStdString()
                    );
            }

            std::vector<Application::CalendarEventSeriesEditOccurrenceSnapshot>
                occurrences;
            occurrences.reserve(
                static_cast<std::size_t>(seriesEvents->size())
                );
            for (const CalendarEvent& seriesEvent : *seriesEvents)
            {
                occurrences.push_back({
                    .eventId = seriesEvent.id,
                    .startDate = seriesEvent.startDate
                        .toString(Qt::ISODate)
                        .toStdString()
                });
            }

            const Application::CalendarEventSeriesEditPlanResult plan =
                Application::planCalendarEventSeriesEditSuffix(
                    request,
                    occurrences
                    );
            if (!plan)
            {
                return failure(
                    plan.error().code,
                    plan.error().message
                    );
            }

            const std::vector<
                Application::CalendarEventSeriesEditOccurrenceUpdate>& updates =
                plan.value();
            if (updates.size()
                != static_cast<std::size_t>(seriesEvents->size()))
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Calendar repeat-series edit plan did not match its source events."
                    );
            }

            QList<CalendarEvent> updatedEvents;
            updatedEvents.reserve(seriesEvents->size());
            for (std::size_t index = 0; index < updates.size(); ++index)
            {
                const CalendarEvent& seriesEvent = seriesEvents->at(
                    static_cast<qsizetype>(index)
                    );
                const Application::CalendarEventSeriesEditOccurrenceUpdate& update =
                    updates.at(index);
                if (update.eventId != seriesEvent.id)
                {
                    return failure(
                        Domain::ErrorCode::Technical,
                        "Calendar repeat-series edit plan changed an occurrence identity."
                        );
                }

                CalendarEvent updatedEvent = seriesEvent;
                updatedEvent.title = legacyText(update.title);
                updatedEvent.eventType = legacyText(update.eventType);
                updatedEvent.timeStatus = legacyText(update.timeStatus);
                updatedEvent.allDay = update.allDay;
                updatedEvent.startTime = update.startTime.has_value()
                    ? legacyTime(*update.startTime)
                    : QTime();
                updatedEvent.endTime = update.endTime.has_value()
                    ? legacyTime(*update.endTime)
                    : QTime();
                updatedEvent.repeatSeriesId = legacyText(
                    update.repeatSeriesId
                    ).trimmed();
                updatedEvent.startDate = legacyDate(update.startDate);
                updatedEvent.endDate = legacyDate(update.endDate);
                if (!updatedEvent.startDate.isValid()
                    || !updatedEvent.endDate.isValid())
                {
                    return failure(
                        Domain::ErrorCode::Technical,
                        "Calendar repeat-series edit produced an invalid occurrence date."
                        );
                }

                if ((update.startTime.has_value()
                     && !updatedEvent.startTime.isValid())
                    || (update.endTime.has_value()
                        && !updatedEvent.endTime.isValid()))
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Calendar repeat-series edit times must be valid."
                        );
                }

                updatedEvents.append(std::move(updatedEvent));
            }

            const ::Result<QList<int>> saved =
                service->saveEvents(updatedEvents);
            if (!saved)
            {
                if (!service->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The calendar service is unavailable."
                        );
                }

                const QByteArray errorBytes = saved.error().toUtf8();
                return failure(
                    Domain::ErrorCode::Technical,
                    errorBytes.isEmpty()
                        ? "Calendar repeat series could not be saved."
                        : errorBytes.toStdString()
                    );
            }

            return Application::CalendarEventSeriesEditResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar repeat series could not be edited."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Calendar repeat series could not be edited."
                );
        }
    }

private:
    [[nodiscard]] static QString legacyText(
        const std::string& value
        )
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static QDate legacyDate(
        const std::string& value
        )
    {
        const QString date = legacyText(value);
        const QDate parsed = QDate::fromString(date, Qt::ISODate);
        return parsed.isValid()
                && parsed.toString(Qt::ISODate) == date
            ? parsed
            : QDate();
    }

    [[nodiscard]] static QTime legacyTime(
        const std::string& value
        )
    {
        const QString time = legacyText(value);
        const QTime parsed = QTime::fromString(
            time,
            QStringLiteral("HH:mm")
            );
        return parsed.isValid()
                && parsed.toString(QStringLiteral("HH:mm")) == time
            ? parsed
            : QTime();
    }

    [[nodiscard]] static Application::CalendarEventSeriesEditResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::CalendarEventSeriesEditResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform

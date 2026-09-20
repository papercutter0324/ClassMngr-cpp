#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/calendar_event_series_edit_port.h"

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QTime>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for editing a repeat-series suffix. The legacy service
// owns recurrence selection and persistence; this adapter only copies the
// bounded request into legacy values and returns an owned typed result.
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

            const int startDateOffset =
                startDate.daysTo(editedStartDate);
            const int durationDays =
                editedStartDate.daysTo(editedEndDate);
            const QTime editedStartTime = request.startTime.has_value()
                ? legacyTime(*request.startTime)
                : QTime();
            const QTime editedEndTime = request.endTime.has_value()
                ? legacyTime(*request.endTime)
                : QTime();
            if ((request.startTime.has_value()
                 && !editedStartTime.isValid())
                || (request.endTime.has_value()
                    && !editedEndTime.isValid()))
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Calendar repeat-series edit times must be valid."
                    );
            }

            QList<CalendarEvent> updatedEvents;
            updatedEvents.reserve(seriesEvents->size());
            for (const CalendarEvent& seriesEvent : *seriesEvents)
            {
                CalendarEvent updatedEvent = seriesEvent;
                updatedEvent.title = legacyText(request.title);
                updatedEvent.eventType = legacyText(request.eventType);
                updatedEvent.timeStatus = legacyText(request.timeStatus);
                updatedEvent.allDay = request.allDay;
                updatedEvent.startTime = editedStartTime;
                updatedEvent.endTime = editedEndTime;
                updatedEvent.repeatSeriesId = repeatSeriesId;
                updatedEvent.startDate = seriesEvent.startDate.addDays(
                    startDateOffset
                    );
                updatedEvent.endDate = updatedEvent.startDate.addDays(
                    durationDays
                    );
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

#include "calendar_event_repository.h"

#include "classmngr/engine/calendar_event_service.h"
#include "classmngr/engine/open_database.h"

#include <QByteArray>
#include <QObject>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using EngineCalendarDate = classmngr::engine::CalendarDate;
using EngineCalendarEvent = classmngr::engine::CalendarEvent;
using EngineCalendarEventService =
    classmngr::engine::CalendarEventService;
using EngineError = classmngr::engine::Error;

std::string toUtf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(
    std::string_view value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QString operationFailure(
    const QString& operation,
    const QString& detail = {}
    )
{
    QString message = QObject::tr("%1 failed").arg(operation);
    const QString trimmedDetail = detail.trimmed();
    if (!trimmedDetail.isEmpty())
    {
        message += QStringLiteral(": ") + trimmedDetail;
    }
    return message;
}

QString engineErrorDetail(
    const EngineError& error
    )
{
    if (error.code == classmngr::engine::ErrorCode::NotFound)
    {
        return QObject::tr("no matching record exists.");
    }

    const QString detail = fromUtf8(error.message);
    if (!detail.trimmed().isEmpty())
    {
        return detail;
    }

    return QObject::tr("The engine reported a %1 error.")
        .arg(fromUtf8(classmngr::engine::errorCodeName(error.code)));
}

QString engineFailure(
    const QString& operation,
    const EngineError& error
    )
{
    return operationFailure(operation, engineErrorDetail(error));
}

QString boundaryConversionFailure(
    const QString& operation
    )
{
    return operationFailure(
        operation,
        QObject::tr("The calendar event contains an unsupported value.")
        );
}

QList<CalendarEvent> fromEngineEvents(
    const std::vector<EngineCalendarEvent>& source
    )
{
    QList<CalendarEvent> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const EngineCalendarEvent& event : source)
    {
        result.append(calendarEventFromEngine(event));
    }
    return result;
}

std::vector<EngineCalendarEvent> toEngineEvents(
    const QList<CalendarEvent>& source
    )
{
    std::vector<EngineCalendarEvent> result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const CalendarEvent& event : source)
    {
        result.push_back(calendarEventToEngine(event));
    }
    return result;
}
} // namespace

CalendarEventRepository::CalendarEventRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

CalendarEventRepository::~CalendarEventRepository() = default;

Status CalendarEventRepository::ensureEngineDatabase(
    const QString& operation
    )
{
    if (!m_database.isValid() || !m_database.isOpen())
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                QObject::tr("No Teacher Profile is open.")
                )
            );
    }

    const QString databasePath = m_database.databaseName();
    if (databasePath.trimmed().isEmpty()
        || databasePath.trimmed() == QStringLiteral(":memory:"))
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                QObject::tr("No database path is available.")
                )
            );
    }

    if (m_engineDatabase
        && m_engineDatabase->isOpen()
        && m_engineDatabasePath == databasePath)
    {
        return {};
    }

    m_engineDatabase.reset();
    m_engineDatabasePath.clear();

    auto opened = classmngr::engine::OpenDatabase::execute(
        toUtf8(databasePath)
        );
    if (!opened)
    {
        return std::unexpected(engineFailure(operation, opened.error()));
    }
    if (*opened == nullptr)
    {
        return std::unexpected(
            operationFailure(
                operation,
                QObject::tr("The engine database could not be opened.")
                )
            );
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

Result<QList<CalendarEvent>> CalendarEventRepository::loadCalendarEventsForDate(
    const QDate& date
    )
{
    const QString operation = QObject::tr("Loading calendar events for date");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineCalendarEvent>> loaded =
        service.loadForDate(calendar_event_detail::toEngineDate(date));
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    return fromEngineEvents(*loaded);
}

Result<QList<CalendarEvent>> CalendarEventRepository::loadCalendarEventsInRange(
    const QDate& startDate,
    const QDate& endDate
    )
{
    const QString operation = QObject::tr("Loading calendar events in range");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineCalendarEvent>> loaded =
        service.loadInRange(
            calendar_event_detail::toEngineDate(startDate),
            calendar_event_detail::toEngineDate(endDate)
            );
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    return fromEngineEvents(*loaded);
}

Result<QList<CalendarEvent>>
CalendarEventRepository::loadUpcomingCalendarEvents(
    const QDate& fromDate,
    int limit
    )
{
    const QString operation = QObject::tr("Loading upcoming calendar events");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineCalendarEvent>> loaded =
        service.loadUpcoming(
            calendar_event_detail::toEngineDate(fromDate),
            limit
            );
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    return fromEngineEvents(*loaded);
}

Result<QDate> CalendarEventRepository::findNextCalendarEventStartDate(
    const QDate& fromDate
    )
{
    const QString operation =
        QObject::tr("Finding next calendar event start date");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Result<std::optional<EngineCalendarDate>> loaded =
        service.findNextStartDate(
            calendar_event_detail::toEngineDate(fromDate)
            );
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }
    if (!loaded->has_value())
    {
        return QDate{};
    }

    const QDate converted = calendar_event_detail::fromEngineDate(**loaded);
    if (!converted.isValid())
    {
        return std::unexpected(boundaryConversionFailure(operation));
    }
    return converted;
}

Result<CalendarEvent> CalendarEventRepository::getCalendarEvent(
    int eventId
    )
{
    const QString operation = QObject::tr("Loading calendar event");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineCalendarEvent> loaded = service.get(
        eventId
        );
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    return calendarEventFromEngine(*loaded);
}

Result<QList<CalendarEvent>>
CalendarEventRepository::loadCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    const QString operation =
        QObject::tr("Loading calendar repeat series events");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineCalendarEvent>> loaded =
        service.loadRepeatSeriesFromDate(
            toUtf8(repeatSeriesId),
            calendar_event_detail::toEngineDate(startDate)
            );
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    return fromEngineEvents(*loaded);
}

Result<int> CalendarEventRepository::saveCalendarEvent(
    const CalendarEvent& event
    )
{
    const QString operation = event.id > 0
        ? QObject::tr("Updating calendar event")
        : QObject::tr("Creating calendar event");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Result<int> saved = service.save(
        calendarEventToEngine(event)
        );
    if (!saved)
    {
        return std::unexpected(engineFailure(operation, saved.error()));
    }

    return *saved;
}

Result<QList<int>> CalendarEventRepository::saveCalendarEvents(
    const QList<CalendarEvent>& events
    )
{
    const QString operation = QObject::tr("Saving calendar event batch");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<int>> saved = service.saveBatch(
        toEngineEvents(events)
        );
    if (!saved)
    {
        return std::unexpected(engineFailure(operation, saved.error()));
    }

    QList<int> result;
    result.reserve(static_cast<qsizetype>(saved->size()));
    for (const int eventId : *saved)
    {
        result.append(eventId);
    }
    return result;
}

Status CalendarEventRepository::deleteCalendarEvent(
    int eventId
    )
{
    const QString operation = QObject::tr("Deleting calendar event");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Status deleted = service.remove(eventId);
    if (!deleted)
    {
        return std::unexpected(engineFailure(operation, deleted.error()));
    }
    return {};
}

Status CalendarEventRepository::deleteCalendarEventsForRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    )
{
    const QString operation =
        QObject::tr("Deleting calendar repeat series events");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Status deleted =
        service.removeRepeatSeriesFromDate(
            toUtf8(repeatSeriesId),
            calendar_event_detail::toEngineDate(startDate)
            );
    if (!deleted)
    {
        return std::unexpected(engineFailure(operation, deleted.error()));
    }
    return {};
}

Status CalendarEventRepository::deleteAllCalendarEvents()
{
    const QString operation = QObject::tr("Deleting all calendar events");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineCalendarEventService service(*m_engineDatabase);
    const classmngr::engine::Status deleted = service.removeAll();
    if (!deleted)
    {
        return std::unexpected(engineFailure(operation, deleted.error()));
    }
    return {};
}

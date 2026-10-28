#include "next/platform/workspace_store_calendar_import_actions.h"

#include "data/database/workspace_store.h"
#include "data/repositories/calendar_event_repository.h"
#include "domain/models/calendar_event.h"
#include "next/application/calendar_event_import_save_port.h"
#include "next/application/calendar_event_import_signature_query_port.h"
#include "next/platform/calendar_event_time_status_adapter.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QDate>
#include <QTime>

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{
namespace
{

using namespace Application;

CalendarEventImportSignature signatureForRecord(
    const CalendarEventImportSignatureSourceRecord& record
    )
{
    return CalendarEventImportSignature::fromNormalizedFields({
        .simplifiedTitle = record.title.simplified().toStdU16String(),
        .normalizedEventType =
            normalizedCalendarEventType(record.eventType).toStdU16String(),
        .startDateIso = record.startDate.toString(Qt::ISODate).toStdU16String(),
        .endDateIso = record.endDate.toString(Qt::ISODate).toStdU16String(),
        .allDay = record.allDay,
        .normalizedTimeStatus =
            normalizedCalendarEventTimeStatus(record.timeStatus).toStdU16String()
    });
}

CalendarEventImportSignatureQueryResult failureQuery(
    const Domain::ErrorCode code,
    std::string message
    )
{
    return CalendarEventImportSignatureQueryResult::failure({
        .code = code,
        .message = std::move(message),
        .recoverable = code == Domain::ErrorCode::NotFound
    });
}

CalendarEventImportSaveResult failureSave(
    const Domain::ErrorCode code,
    std::string message
    )
{
    return CalendarEventImportSaveResult::failure({
        .code = code,
        .message = std::move(message),
        .recoverable = code == Domain::ErrorCode::NotFound
    });
}

CalendarEventImportSaveResult cancelledSave()
{
    return failureSave(
        Domain::ErrorCode::NotFound,
        "The active workspace changed during Calendar import."
        );
}

class WorkspaceStoreCalendarImportPorts final
    : public CalendarEventImportSignatureQueryPort,
      public CalendarEventImportSavePort
{
public:
    WorkspaceStoreCalendarImportPorts(
        WorkspaceStore& store,
        std::function<bool()> mayContinue
        )
        : m_store(&store),
          m_mayContinue(std::move(mayContinue))
    {
    }

    [[nodiscard]] bool isAvailable() const noexcept override
    {
        return canContinue() && m_store && m_store->isOpen()
            && m_store->calendarEventRepository();
    }

    [[nodiscard]] CalendarEventImportSignatureQueryResult
    loadSignaturesInRange(
        const CalendarEventImportSignatureRangeRequest& request
        ) const override
    {
        if (!canContinue())
        {
            return failureQuery(
                Domain::ErrorCode::NotFound,
                "The active workspace changed during Calendar import."
                );
        }
        if (!m_store || !m_store->isOpen())
        {
            return failureQuery(
                Domain::ErrorCode::NotFound,
                "The Calendar workspace is unavailable."
                );
        }

        const QDate startDate = QDate::fromString(
            QString::fromStdString(request.startDate.value()), Qt::ISODate);
        const QDate endDate = QDate::fromString(
            QString::fromStdString(request.endDate.value()), Qt::ISODate);
        if (!request.startDate.isValid() || !request.endDate.isValid()
            || !startDate.isValid() || !endDate.isValid()
            || endDate < startDate)
        {
            return failureQuery(
                Domain::ErrorCode::InvalidInput,
                "Calendar event range must contain valid ordered dates."
                );
        }

        CalendarEventRepository* const repository =
            m_store->calendarEventRepository();
        if (!repository)
        {
            return failureQuery(
                Domain::ErrorCode::NotFound,
                "The Calendar event repository is unavailable."
                );
        }

        CalendarEventImportSignatureKeys signatures;
        const RepositoryStatus visited =
            repository->visitCalendarEventImportSignatureSourcesInRange(
                startDate,
                endDate,
                [&signatures](
                    const CalendarEventImportSignatureSourceRecord& record
                    )
                {
                    signatures.push_back(signatureForRecord(record));
                    return CalendarEventRangeVisitDecision::Continue;
                }
                );
        if (!visited)
        {
            const QByteArray diagnostic = visited.error().diagnostic.toUtf8();
            return failureQuery(
                repositoryDomainErrorCode(visited.error().code),
                diagnostic.isEmpty()
                    ? "Calendar event signatures could not be loaded."
                    : diagnostic.toStdString()
                );
        }

        if (!canContinue())
        {
            return failureQuery(
                Domain::ErrorCode::NotFound,
                "The active workspace changed during Calendar import."
                );
        }

        return CalendarEventImportSignatureQueryResult::success(
            std::move(signatures));
    }

    [[nodiscard]] CalendarEventImportSaveResult saveImportedEvents(
        const CalendarEventImportSaveRequest& request
        ) override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return failureSave(
                validation.error().code,
                validation.error().message
                );
        }
        if (!m_store || !m_store->isOpen())
        {
            return failureSave(
                Domain::ErrorCode::NotFound,
                "The Calendar workspace is unavailable."
                );
        }

        QList<CalendarEvent> events;
        events.reserve(static_cast<qsizetype>(request.events.size()));
        for (const CalendarEventSaveRequest& input : request.events)
        {
            const auto event = toLegacyEvent(input);
            if (!event)
            {
                return failureSave(
                    Domain::ErrorCode::InvalidInput,
                    "Calendar import event contains invalid date or time values."
                    );
            }
            events.append(*event);
        }

        CalendarEventRepository* const repository =
            m_store->calendarEventRepository();
        if (!repository)
        {
            return failureSave(
                Domain::ErrorCode::NotFound,
                "The Calendar event repository is unavailable."
                );
        }
        // The page/path cancellation check is the last operation before the
        // repository starts the one transaction for the complete batch.
        if (!canContinue())
        {
            return cancelledSave();
        }

        const RepositoryResult<QList<qint64>> saved =
            repository->saveCalendarEvents(events);
        if (!saved)
        {
            const QByteArray diagnostic = saved.error().diagnostic.toUtf8();
            return failureSave(
                repositoryDomainErrorCode(saved.error().code),
                diagnostic.isEmpty()
                    ? "Calendar import events could not be saved."
                    : diagnostic.toStdString()
                );
        }
        if (saved->size() != static_cast<qsizetype>(request.events.size()))
        {
            return failureSave(
                Domain::ErrorCode::Technical,
                "Calendar import save did not return one identifier per event."
                );
        }

        std::vector<Domain::CalendarEventId> ids;
        ids.reserve(static_cast<std::size_t>(saved->size()));
        for (const qint64 id : *saved)
        {
            const auto typed = Domain::CalendarEventId::fromString(
                std::to_string(id));
            if (id <= 0 || !typed)
            {
                return failureSave(
                    Domain::ErrorCode::Technical,
                    "Calendar import save returned an invalid event identifier."
                    );
            }
            ids.push_back(*typed);
        }
        return CalendarEventImportSaveResult::success(std::move(ids));
    }

private:
    [[nodiscard]] bool canContinue() const noexcept
    {
        if (!m_mayContinue)
        {
            return true;
        }
        try
        {
            return m_mayContinue();
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] static QString text(const std::string& value)
    {
        return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    }

    [[nodiscard]] static std::optional<CalendarEvent> toLegacyEvent(
        const CalendarEventSaveRequest& request
        )
    {
        if (request.id.has_value())
        {
            return std::nullopt;
        }

        const QDate startDate(request.startDate.year(), request.startDate.month(),
                              request.startDate.day());
        const QDate endDate(request.endDate.year(), request.endDate.month(),
                            request.endDate.day());
        if (!startDate.isValid() || !endDate.isValid() || endDate < startDate)
        {
            return std::nullopt;
        }

        CalendarEvent event;
        event.title = text(request.title).simplified();
        event.eventType = text(request.eventType).trimmed();
        event.timeStatus = calendarEventTimeStatusToLegacyText(request.timeStatus);
        event.allDay = request.allDay;
        event.startDate = startDate;
        event.endDate = endDate;
        if (request.startTime.has_value())
        {
            const QString startText = text(*request.startTime);
            const QString endText = text(*request.endTime);
            const QTime startTime = QTime::fromString(startText, QStringLiteral("HH:mm"));
            const QTime endTime = QTime::fromString(endText, QStringLiteral("HH:mm"));
            if (!startTime.isValid() || !endTime.isValid()
                || startTime.toString(QStringLiteral("HH:mm")) != startText
                || endTime.toString(QStringLiteral("HH:mm")) != endText)
            {
                return std::nullopt;
            }
            event.startTime = startTime;
            event.endTime = endTime;
        }
        event.repeatSeriesId.clear();
        return event;
    }

    WorkspaceStore* m_store = nullptr;
    std::function<bool()> m_mayContinue;
};

Domain::Result<CalendarEventImportUseCaseResult> failureResult(
    const Domain::ErrorCode code,
    std::string message,
    const bool recoverable = false
    )
{
    return Domain::Result<CalendarEventImportUseCaseResult>::failure({
        .code = code,
        .message = std::move(message),
        .recoverable = recoverable
    });
}

} // namespace

Domain::Result<CalendarEventImportUseCaseResult> applyCalendarEventImportAtPath(
    const QString& databasePath,
    const CalendarEventImportUseCaseRequest& request,
    std::function<bool()> mayContinue
    )
{
    if (databasePath.trimmed().isEmpty())
    {
        return failureResult(
            Domain::ErrorCode::NotFound,
            "Open a workspace before importing Calendar events.",
            true
            );
    }
    if (mayContinue)
    {
        try
        {
            if (!mayContinue())
            {
                return failureResult(
                    Domain::ErrorCode::NotFound,
                    "The active workspace changed during Calendar import.",
                    true
                    );
            }
        }
        catch (...)
        {
            return failureResult(
                Domain::ErrorCode::NotFound,
                "The Calendar import operation is no longer active.",
                true
                );
        }
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        const QByteArray diagnostic = toLegacyDiagnostic(opened.error()).toUtf8();
        return failureResult(
            repositoryDomainErrorCode(opened.error().code),
            diagnostic.isEmpty()
                ? "The Calendar workspace could not be opened."
                : diagnostic.toStdString(),
            opened.error().recoverableOverride.value_or(
                opened.error().code != RepositoryErrorCode::Technical)
            );
    }

    try
    {
        WorkspaceStoreCalendarImportPorts ports(store, std::move(mayContinue));
        return CalendarEventImportUseCase::execute(request, ports, ports);
    }
    catch (const std::exception&)
    {
        return failureResult(
            Domain::ErrorCode::Technical,
            "Calendar import could not be completed."
            );
    }
    catch (...)
    {
        return failureResult(
            Domain::ErrorCode::Technical,
            "Calendar import could not be completed."
            );
    }
}

} // namespace ClassMngr::Next::Platform

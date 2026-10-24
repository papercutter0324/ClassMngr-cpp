#include "calendar_event_import_service.h"

#include "app/services/feature_services.h"
#include "academic_calendar_event_parser.h"
#include "core/application_services.h"
#include "core/startup_profiler.h"
#include "next/application/calendar_event_import_count.h"
#include "next/application/calendar_event_import_campus_code_query_port.h"
#include "next/application/calendar_event_import_use_case.h"
#include "next/application/calendar_event_import_signature_query_port.h"
#include "next/platform/calendar_event_import_campus_code_query_adapter.h"
#include "next/platform/application_services_calendar_event_import_save_port.h"
#include "next/platform/calendar_event_time_status_adapter.h"
#include "next/platform/application_services_calendar_event_import_signature_query_port.h"

#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QUrl>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace
{
const QString ImportUrl =
    QStringLiteral(
        "https://docs.google.com/spreadsheets/d/"
        "18O05g7nlnsoUwrWhArZkptJFp3LMbSytdgKDjNaoMU4/"
        "export?format=xlsx&gid=570696063"
        );

struct CalendarImportOperationReleaseGuard
{
    ~CalendarImportOperationReleaseGuard()
    {
        StartupProfiler::recordCalendarImportOperationReleased();
    }
};

class CalendarImportProfilerObserver final
    : public ClassMngr::Next::Application::
        CalendarEventImportUseCaseObserver
{
public:
    void existingSignaturesLoaded(const std::size_t count) override
    {
        StartupProfiler::recordCalendarImportExistingEventsLoaded(
            static_cast<int>(count)
            );
    }

    void savePrepared(
        const std::size_t eventCount,
        const ClassMngr::Next::Application::CalendarEventImportCount
            skippedCount
        ) override
    {
        StartupProfiler::recordCalendarImportSavePrepared(
            static_cast<int>(eventCount),
            skippedCount.rawValue()
            );
    }
};

QStringList calendarImportCampusCodes(
    const ClassMngr::Next::Application::
        CalendarEventImportCampusCodeQueryPort& campusCodeQueryPort
    )
{
    using CampusCode = ClassMngr::Next::Application::CampusCode;
    QStringList codes;
    for (const CampusCode& campusCode :
         campusCodeQueryPort.loadCampusCodes())
    {
        const std::string& text = campusCode.value();
        codes.append(
            QString::fromUtf8(
                text.data(),
                static_cast<qsizetype>(text.size())
                )
            );
    }

    return codes;
}

ClassMngr::Next::Application::CalendarEventImportSignature
calendarEventSignatureKey(const CalendarEvent& event)
{
    return CalendarImport::calendarEventImportSignature(event);
}

ClassMngr::Next::Application::CalendarEventSaveInput
calendarEventImportSaveInput(const CalendarEvent& event)
{
    ClassMngr::Next::Application::CalendarEventSaveInput request;
    request.title = event.title.toUtf8().toStdString();
    request.startDate = event.startDate.toString(Qt::ISODate).toStdString();
    request.endDate = event.endDate.toString(Qt::ISODate).toStdString();
    request.allDay = event.allDay;
    request.eventType = event.eventType.toUtf8().toStdString();
    request.timeStatus =
        ClassMngr::Next::Platform::calendarEventTimeStatusFromLegacyText(
            event.timeStatus
            );
    if (event.startTime.isValid())
    {
        request.startTime =
            event.startTime.toString(QStringLiteral("HH:mm")).toStdString();
    }
    if (event.endTime.isValid())
    {
        request.endTime =
            event.endTime.toString(QStringLiteral("HH:mm")).toStdString();
    }

    return request;
}
}

CalendarEventImportService::CalendarEventImportService(
    ApplicationServices* services,
    QObject* parent
    )
    : QObject(parent)
    , m_services(services)
    , m_network(new QNetworkAccessManager(this))
{
    connect(
        m_network,
        &QNetworkAccessManager::finished,
        this,
        &CalendarEventImportService::handleFinished
        );
}

QString CalendarEventImportService::defaultImportUrl()
{
    return ImportUrl;
}

bool CalendarEventImportService::isImporting() const
{
    return m_importing;
}

void CalendarEventImportService::importFromDefaultSource()
{
    if (m_importing)
    {
        return;
    }

    if (!m_services)
    {
        emit importFailed(
            tr("The calendar Teacher Profile is not available.")
        );
        return;
    }

    const ClassMngr::Next::Platform::
        ApplicationServicesCalendarEventImportSignatureQueryPort
            signatureQueryAdapter(*m_services);
    const ClassMngr::Next::Application::
        CalendarEventImportSignatureQueryPort& signatureQueryPort =
            signatureQueryAdapter;
    if (!signatureQueryPort.isAvailable())
    {
        emit importFailed(
            tr("The calendar Teacher Profile is not available.")
            );
        return;
    }

    const QString configuredUrl =
        qEnvironmentVariable(
            "CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL"
            ).trimmed();
    const QUrl importUrl(
        configuredUrl.isEmpty()
            ? ImportUrl
            : configuredUrl
        );
    if (!importUrl.isValid() || importUrl.scheme().isEmpty())
    {
        emit importFailed(
            tr("The calendar import source URL is invalid.")
            );
        return;
    }

    QNetworkRequest request{importUrl};
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );

    auto* stagedWorkbook = new QTemporaryFile(this);
    if (!stagedWorkbook->open())
    {
        const QString message = stagedWorkbook->errorString();
        delete stagedWorkbook;
        StartupProfiler::recordCalendarImportFailed(message);
        emit importFailed(message);
        return;
    }

    m_importing = true;
    StartupProfiler::recordCalendarImportStarted(
        importUrl.toString()
        );
    QNetworkReply* reply = m_network->get(request);
    m_stagedWorkbooks.insert(reply, stagedWorkbook);
    connect(
        reply,
        &QNetworkReply::readyRead,
        this,
        [this, reply]()
        {
            handleReadyRead(reply);
        }
        );
}

void CalendarEventImportService::handleReadyRead(
    QNetworkReply* reply
    )
{
    QTemporaryFile* stagedWorkbook = m_stagedWorkbooks.value(reply, nullptr);
    if (!stagedWorkbook || m_stagingErrors.contains(reply))
    {
        return;
    }

    constexpr qint64 TransferChunkSize = 64 * 1024;
    while (reply->bytesAvailable() > 0)
    {
        const qint64 requestedSize = qMin(
            TransferChunkSize,
            reply->bytesAvailable()
            );
        const QByteArray chunk = reply->read(requestedSize);
        if (chunk.isEmpty())
        {
            break;
        }

        qint64 bytesWritten = 0;
        while (bytesWritten < chunk.size())
        {
            const qint64 count = stagedWorkbook->write(
                chunk.constData() + bytesWritten,
                chunk.size() - bytesWritten
                );
            if (count <= 0)
            {
                QString message = stagedWorkbook->errorString();
                if (message.isEmpty())
                {
                    message = tr("The downloaded spreadsheet could not be staged.");
                }
                m_stagingErrors.insert(reply, message);
                reply->abort();
                return;
            }
            bytesWritten += count;
        }
    }
}

void CalendarEventImportService::handleFinished(
    QNetworkReply* reply
    )
{
    handleReadyRead(reply);
    QTemporaryFile* stagedWorkbook = m_stagedWorkbooks.take(reply);
    const QString stagingError = m_stagingErrors.take(reply);
    std::unique_ptr<QTemporaryFile> stagedWorkbookGuard(stagedWorkbook);
    if (stagedWorkbook)
    {
        stagedWorkbook->setParent(nullptr);
    }

    reply->deleteLater();
    m_importing = false;
    const CalendarImportOperationReleaseGuard releaseGuard;

    if (!stagingError.isEmpty())
    {
        StartupProfiler::recordCalendarImportFailed(stagingError);
        emit importFailed(stagingError);
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        StartupProfiler::recordCalendarImportFailed(
            reply->errorString()
            );
        emit importFailed(
            reply->errorString()
            );
        return;
    }

    if (!stagedWorkbook || !stagedWorkbook->flush())
    {
        const QString message = stagedWorkbook
            ? stagedWorkbook->errorString()
            : tr("The downloaded spreadsheet could not be staged.");
        StartupProfiler::recordCalendarImportFailed(message);
        emit importFailed(message);
        return;
    }

    StartupProfiler::recordCalendarImportResponseReceived(
        stagedWorkbook->size()
        );

    if (!stagedWorkbook->seek(0))
    {
        const QString message = stagedWorkbook->errorString();
        StartupProfiler::recordCalendarImportFailed(message);
        emit importFailed(message);
        return;
    }

    QString errorMessage;
    const ClassMngr::Next::Platform::
        CalendarEventImportCampusCodeQueryAdapter campusCodeQueryAdapter;
    const ClassMngr::Next::Application::
        CalendarEventImportCampusCodeQueryPort& campusCodeQueryPort =
            campusCodeQueryAdapter;
    CalendarImport::CalendarWorkbookReadCounts readCounts;
    CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbookDevice(
            *stagedWorkbook,
            calendarImportCampusCodes(campusCodeQueryPort),
            &errorMessage,
            &readCounts
            );
    const bool workbookParsed = errorMessage.isEmpty();

    if (!workbookParsed)
    {
        parsed = {};
        StartupProfiler::recordCalendarImportFailed(errorMessage);
        emit importFailed(errorMessage);
        return;
    }

    StartupProfiler::recordCalendarImportWorkbookParsed(
        readCounts.worksheetCount,
        readCounts.cellCount,
        readCounts.mergedRangeCount,
        readCounts.sharedStringCount,
        readCounts.styleCount
        );

    StartupProfiler::recordCalendarImportEventsPrepared(
        parsed.events.size(),
        parsed.skippedCount.rawValue()
        );

    if (parsed.events.isEmpty())
    {
        emit importFinished(
            0,
            parsed.skippedCount.rawValue()
            );
        return;
    }

    QDate firstDate =
        parsed.events.first().startDate;
    QDate lastDate =
        parsed.events.first().endDate;

    for (const CalendarEvent& event : parsed.events)
    {
        firstDate =
            qMin(firstDate, event.startDate);
        lastDate =
            qMax(lastDate, event.endDate);
    }

    if (!m_services)
    {
        const QString message =
            tr("The calendar Teacher Profile is not available.");
        StartupProfiler::recordCalendarImportFailed(message);
        emit importFailed(message);
        return;
    }

    ClassMngr::Next::Application::CalendarEventImportUseCaseRequest
        useCaseRequest;
    useCaseRequest.signatureRange = {
        .startDate = ClassMngr::Next::Application::CalendarEventDate(
            firstDate.toString(Qt::ISODate).toStdString()
            ),
        .endDate = ClassMngr::Next::Application::CalendarEventDate(
            lastDate.toString(Qt::ISODate).toStdString()
            )
    };
    useCaseRequest.initiallySkippedCount = parsed.skippedCount;
    useCaseRequest.candidates.reserve(
        static_cast<std::size_t>(parsed.events.size())
        );
    for (const CalendarEvent& event : parsed.events)
    {
        useCaseRequest.candidates.push_back({
            .signature = calendarEventSignatureKey(event),
            .saveInput = calendarEventImportSaveInput(event)
        });
    }

    const ClassMngr::Next::Platform::
        ApplicationServicesCalendarEventImportSignatureQueryPort
            signatureQueryAdapter(*m_services);
    const ClassMngr::Next::Application::
        CalendarEventImportSignatureQueryPort& signatureQueryPort =
            signatureQueryAdapter;

    ClassMngr::Next::Platform::
        ApplicationServicesCalendarEventImportSavePort importSavePort(
            *m_services
            );
    CalendarImportProfilerObserver profilerObserver;
    const auto imported =
        ClassMngr::Next::Application::CalendarEventImportUseCase::execute(
            useCaseRequest,
            signatureQueryPort,
            importSavePort,
            &profilerObserver
            );
    if (!imported)
    {
        const std::string& errorText = imported.error().message;
        const QString message = QString::fromUtf8(
            errorText.data(),
            static_cast<qsizetype>(errorText.size())
            );
        StartupProfiler::recordCalendarImportFailed(message);
        emit importFailed(message);
        return;
    }

    StartupProfiler::recordCalendarImportApplied(
        imported.value().importedCount.rawValue(),
        imported.value().skippedCount.rawValue()
        );

    emit importFinished(
        imported.value().importedCount.rawValue(),
        imported.value().skippedCount.rawValue()
        );
}

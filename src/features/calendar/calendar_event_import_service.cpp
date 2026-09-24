#include "calendar_event_import_service.h"

#include "app/services/feature_services.h"
#include "academic_calendar_event_parser.h"
#include "calendar_workbook_reader.h"
#include "core/application_services.h"
#include "core/startup_profiler.h"
#include "next/application/calendar_event_import_campus_code_query_port.h"
#include "next/application/calendar_event_import_use_case.h"
#include "next/application/calendar_event_import_signature_query_port.h"
#include "next/platform/calendar_event_import_campus_code_query_adapter.h"
#include "next/platform/application_services_calendar_event_import_save_port.h"
#include "next/platform/application_services_calendar_event_import_signature_query_port.h"

#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <cstddef>
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
        const int skippedCount
        ) override
    {
        StartupProfiler::recordCalendarImportSavePrepared(
            static_cast<int>(eventCount),
            skippedCount
            );
    }
};

QStringList calendarImportCampusCodes(
    const ClassMngr::Next::Application::
        CalendarEventImportCampusCodeQueryPort& campusCodeQueryPort
    )
{
    QStringList codes;
    for (const std::string& campusCode :
         campusCodeQueryPort.loadCampusCodes())
    {
        codes.append(
            QString::fromUtf8(
                campusCode.data(),
                static_cast<qsizetype>(campusCode.size())
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

ClassMngr::Next::Application::CalendarEventSaveRequest
calendarEventImportSaveRequest(const CalendarEvent& event)
{
    ClassMngr::Next::Application::CalendarEventSaveRequest request;
    request.title = event.title.toUtf8().toStdString();
    request.startDate = event.startDate.toString(Qt::ISODate).toStdString();
    request.endDate = event.endDate.toString(Qt::ISODate).toStdString();
    request.allDay = event.allDay;
    request.eventType = event.eventType.toUtf8().toStdString();
    request.timeStatus = event.timeStatus.toUtf8().toStdString();
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

    m_importing = true;
    StartupProfiler::recordCalendarImportStarted(
        importUrl.toString()
        );
    m_network->get(request);
}

void CalendarEventImportService::handleFinished(
    QNetworkReply* reply
    )
{
    reply->deleteLater();
    m_importing = false;
    const CalendarImportOperationReleaseGuard releaseGuard;

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

    QByteArray workbookData = reply->readAll();
    StartupProfiler::recordCalendarImportResponseReceived(
        workbookData.size()
        );

    QString errorMessage;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(
            workbookData,
            &errorMessage
            );

    if (!errorMessage.isEmpty())
    {
        StartupProfiler::recordCalendarImportFailed(errorMessage);
        emit importFailed(errorMessage);
        return;
    }

    workbookData.clear();
    workbookData.squeeze();

    int workbookCellCount = 0;
    int workbookMergedRangeCount = 0;
    for (const CalendarImport::Worksheet& worksheet : workbook.worksheets)
    {
        workbookCellCount += worksheet.cells.size();
        workbookMergedRangeCount += worksheet.mergedRanges.size();
    }
    StartupProfiler::recordCalendarImportWorkbookParsed(
        workbook.worksheets.size(),
        workbookCellCount,
        workbookMergedRangeCount,
        workbook.sharedStrings.size(),
        workbook.styles.size()
        );

    const ClassMngr::Next::Platform::
        CalendarEventImportCampusCodeQueryAdapter campusCodeQueryAdapter;
    const ClassMngr::Next::Application::
        CalendarEventImportCampusCodeQueryPort& campusCodeQueryPort =
            campusCodeQueryAdapter;
    CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(
            workbook,
            calendarImportCampusCodes(campusCodeQueryPort)
            );
    StartupProfiler::recordCalendarImportEventsPrepared(
        parsed.events.size(),
        parsed.skippedCount
        );

    if (parsed.events.isEmpty())
    {
        emit importFinished(
            0,
            parsed.skippedCount
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
            .saveRequest = calendarEventImportSaveRequest(event)
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
        imported.value().importedCount,
        imported.value().skippedCount
        );

    emit importFinished(
        imported.value().importedCount,
        imported.value().skippedCount
        );
}

#include "calendar_event_import_service.h"

#include "app/services/feature_services.h"
#include "academic_calendar_event_parser.h"
#include "calendar_workbook_reader.h"
#include "core/resource_paths.h"
#include "core/startup_profiler.h"
#include "features/campus/data/campus_json_repository.h"

#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>

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

QStringList campusCodesFromDirectory()
{
    QStringList codes;
    const CampusJsonRepository repository(
        ResourcePaths::Campuses::directory()
        );

    for (const CampusInfo& campus : repository.loadCampuses())
    {
        const QString code =
            campus.campusCode.trimmed();

        if (!code.isEmpty())
        {
            codes.append(code);
        }
    }

    codes.removeDuplicates();
    return codes;
}
}

CalendarEventImportService::CalendarEventImportService(
    CalendarService* calendarService,
    QObject* parent
    )
    : QObject(parent)
    , m_calendarService(calendarService)
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

    if (!m_calendarService || !m_calendarService->isAvailable())
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

    CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(
            workbook,
            campusCodesFromDirectory()
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

    QSet<QString> existingSignatures;
    const Result<QList<CalendarEvent>> existingEvents =
        m_calendarService->eventsInRange(
            firstDate,
            lastDate
            );
    if (!existingEvents)
    {
        StartupProfiler::recordCalendarImportFailed(existingEvents.error());
        emit importFailed(existingEvents.error());
        return;
    }

    StartupProfiler::recordCalendarImportExistingEventsLoaded(
        existingEvents->size()
        );

    for (const CalendarEvent& event : *existingEvents)
    {
        existingSignatures.insert(
            CalendarImport::calendarEventImportSignature(event)
            );
    }

    QList<CalendarEvent> eventsToSave;
    for (const CalendarEvent& event : parsed.events)
    {
        const QString signature =
            CalendarImport::calendarEventImportSignature(event);

        if (existingSignatures.contains(signature))
        {
            ++parsed.skippedCount;
            continue;
        }

        eventsToSave.append(event);
        existingSignatures.insert(signature);
    }

    StartupProfiler::recordCalendarImportSavePrepared(
        eventsToSave.size(),
        parsed.skippedCount
        );

    const Result<QList<int>> saved =
        m_calendarService->saveEvents(eventsToSave);
    if (!saved)
    {
        StartupProfiler::recordCalendarImportFailed(saved.error());
        emit importFailed(saved.error());
        return;
    }

    StartupProfiler::recordCalendarImportApplied(
        saved->size(),
        parsed.skippedCount
        );

    emit importFinished(
        saved->size(),
        parsed.skippedCount
        );
}

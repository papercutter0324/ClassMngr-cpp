#include "calendar_event_import_service.h"

#include "app/services/feature_services.h"
#include "academic_calendar_event_parser.h"
#include "calendar_workbook_reader.h"
#include "core/resource_paths.h"
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

    QNetworkRequest request{
        QUrl(ImportUrl)
    };
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );

    m_importing = true;
    m_network->get(request);
}

void CalendarEventImportService::handleFinished(
    QNetworkReply* reply
    )
{
    reply->deleteLater();
    m_importing = false;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit importFailed(
            reply->errorString()
            );
        return;
    }

    QString errorMessage;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(
            reply->readAll(),
            &errorMessage
            );

    if (!errorMessage.isEmpty())
    {
        emit importFailed(errorMessage);
        return;
    }

    CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(
            workbook,
            campusCodesFromDirectory()
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
    const QList<CalendarEvent> existingEvents =
        m_calendarService->eventsInRange(
            firstDate,
            lastDate
            );

    for (const CalendarEvent& event : existingEvents)
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

    const Result<QList<int>> saved =
        m_calendarService->saveEvents(eventsToSave);
    if (!saved)
    {
        emit importFailed(saved.error());
        return;
    }

    emit importFinished(
        saved->size(),
        parsed.skippedCount
        );
}

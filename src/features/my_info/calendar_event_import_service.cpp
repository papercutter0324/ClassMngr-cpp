#include "calendar_event_import_service.h"

#include "calendar_event_sheet_parser.h"
#include "calendar_import_workbook.h"
#include "data/data_service.h"

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
}

CalendarEventImportService::CalendarEventImportService(
    DataService* dataService,
    QObject* parent
    )
    : QObject(parent)
    , m_dataService(dataService)
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

    if (!m_dataService || !m_dataService->isOpen())
    {
        emit importFailed(
            tr("The calendar database is not available.")
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
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);

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
        m_dataService->loadCalendarEventsInRange(
            firstDate,
            lastDate
            );

    for (const CalendarEvent& event : existingEvents)
    {
        existingSignatures.insert(
            CalendarImport::calendarEventImportSignature(event)
            );
    }

    int importedCount = 0;
    for (const CalendarEvent& event : parsed.events)
    {
        const QString signature =
            CalendarImport::calendarEventImportSignature(event);

        if (existingSignatures.contains(signature))
        {
            ++parsed.skippedCount;
            continue;
        }

        if (m_dataService->saveCalendarEvent(event) > 0)
        {
            ++importedCount;
            existingSignatures.insert(signature);
        }
        else
        {
            ++parsed.skippedCount;
        }
    }

    emit importFinished(
        importedCount,
        parsed.skippedCount
        );
}

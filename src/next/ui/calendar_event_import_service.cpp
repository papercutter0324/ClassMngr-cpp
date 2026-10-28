#include "next/ui/calendar_event_import_service.h"

#include "domain/models/calendar_event.h"
#include "features/calendar/academic_calendar_event_parser.h"
#include "next/application/calendar_event_import_save_port.h"
#include "next/application/calendar_event_import_use_case.h"
#include "next/platform/calendar_event_time_status_adapter.h"
#include "next/platform/workspace_store_calendar_import_actions.h"

#include <QFileInfo>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QUrl>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

using namespace Application;

constexpr qint64 TransferChunkSize = 64 * 1024;
const QString ImportUrl = QStringLiteral(
    "https://docs.google.com/spreadsheets/d/"
    "18O05g7nlnsoUwrWhArZkptJFp3LMbSytdgKDjNaoMU4/"
    "export?format=xlsx&gid=570696063"
    );

QString text(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString importText(const char* source)
{
    return QCoreApplication::translate("CalendarPreferencesDialog", source);
}

CalendarEventSaveInput saveInputFor(const CalendarEvent& event)
{
    CalendarEventSaveInput input;
    input.title = event.title.toUtf8().toStdString();
    input.startDate = event.startDate.toString(Qt::ISODate).toStdString();
    input.endDate = event.endDate.toString(Qt::ISODate).toStdString();
    input.allDay = event.allDay;
    input.eventType = event.eventType.toUtf8().toStdString();
    input.timeStatus = Platform::calendarEventTimeStatusFromLegacyText(
        event.timeStatus);
    if (event.startTime.isValid())
    {
        input.startTime = event.startTime.toString(QStringLiteral("HH:mm"))
            .toStdString();
    }
    if (event.endTime.isValid())
    {
        input.endTime = event.endTime.toString(QStringLiteral("HH:mm"))
            .toStdString();
    }
    return input;
}

QString sourceUrlFromEnvironment()
{
    const QString configured = qEnvironmentVariable(
        "CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL").trimmed();
    return configured.isEmpty() ? ImportUrl : configured;
}

} // namespace

NextCalendarEventImportService::NextCalendarEventImportService(
    ActiveWorkspacePath activeWorkspacePath,
    CampusCodeProvider campusCodeProvider,
    QString sourceUrlOverride,
    QString temporaryFileTemplate,
    NetworkAccessManagerFactory networkAccessManagerFactory,
    QObject* parent
    )
    : QObject(parent),
      m_activeWorkspacePath(std::move(activeWorkspacePath)),
      m_campusCodeProvider(std::move(campusCodeProvider)),
      m_sourceUrlOverride(std::move(sourceUrlOverride)),
      m_temporaryFileTemplate(std::move(temporaryFileTemplate)),
      m_network(nullptr)
{
    m_network = networkAccessManagerFactory
        ? networkAccessManagerFactory(this)
        : new QNetworkAccessManager(this);
    if (!m_network)
    {
        m_network = new QNetworkAccessManager(this);
    }
    else if (!m_network->parent())
    {
        m_network->setParent(this);
    }
    setObjectName(QStringLiteral("nextCalendarEventImportService"));
    connect(m_network, &QNetworkAccessManager::finished,
            this, &NextCalendarEventImportService::handleFinished);
}

NextCalendarEventImportService::~NextCalendarEventImportService()
{
    m_cancelRequested = true;
    if (m_network)
    {
        disconnect(m_network, nullptr, this, nullptr);
    }
    if (m_reply)
    {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_stagedWorkbook.reset();
    if (!m_stagedWorkbookPath.isEmpty())
    {
        m_metrics.stagedWorkbookRemovedAtTerminal =
            !QFileInfo::exists(m_stagedWorkbookPath);
    }
}

QString NextCalendarEventImportService::defaultImportUrl()
{
    return ImportUrl;
}

bool NextCalendarEventImportService::isImporting() const noexcept
{
    return m_importing;
}

CalendarEventImportRuntimeMetrics
NextCalendarEventImportService::runtimeMetrics() const
{
    return m_metrics;
}

void NextCalendarEventImportService::start(const QString& databasePath)
{
    if (m_importing)
    {
        return;
    }

    m_metrics = {};
    m_cancelRequested = false;
    m_stagingError.clear();
    m_stagedWorkbookPath.clear();
    m_databasePath = databasePath;
    if (databasePath.trimmed().isEmpty())
    {
        fail(importText("Open a workspace before importing Calendar events."));
        return;
    }
    if (!operationIsCurrent())
    {
        canceled();
        return;
    }

    const QUrl url(sourceUrl());
    if (!url.isValid() || url.scheme().isEmpty())
    {
        fail(importText("The Calendar import source URL is invalid."));
        return;
    }

    m_stagedWorkbook = m_temporaryFileTemplate.isEmpty()
        ? std::make_unique<QTemporaryFile>()
        : std::make_unique<QTemporaryFile>(m_temporaryFileTemplate);
    m_stagedWorkbook->setAutoRemove(true);
    if (!m_stagedWorkbook->open())
    {
        const QString message = m_stagedWorkbook->errorString();
        m_stagedWorkbook.reset();
        fail(message.isEmpty()
            ? importText("The downloaded spreadsheet could not be staged.")
            : message);
        return;
    }
    m_stagedWorkbookPath = m_stagedWorkbook->fileName();

    QNetworkRequest request{url};
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    setImporting(true);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]
    {
        if (m_reply)
        {
            handleReadyRead(m_reply);
        }
    });
}

void NextCalendarEventImportService::cancel() noexcept
{
    if (!m_importing)
    {
        return;
    }
    m_cancelRequested = true;
    m_stagedWorkbook.reset();
    if (!m_stagedWorkbookPath.isEmpty())
    {
        m_metrics.stagedWorkbookRemovedOnCancel =
            !QFileInfo::exists(m_stagedWorkbookPath);
    }
    if (m_reply)
    {
        m_reply->abort();
    }
    else
    {
        canceled();
    }
}

void NextCalendarEventImportService::handleReadyRead(QNetworkReply* reply)
{
    if (!m_stagedWorkbook || reply != m_reply || m_cancelRequested
        || !m_stagingError.isEmpty())
    {
        return;
    }

    while (reply->bytesAvailable() > 0)
    {
        const qint64 requested = qMin(TransferChunkSize, reply->bytesAvailable());
        const QByteArray chunk = reply->read(requested);
        if (chunk.isEmpty())
        {
            break;
        }
        m_metrics.downloadedBytes += chunk.size();
        m_metrics.largestTransferChunkBytes = qMax(
            m_metrics.largestTransferChunkBytes,
            static_cast<qint64>(chunk.size()));
        ++m_metrics.transferChunkCount;

        qint64 written = 0;
        while (written < chunk.size())
        {
            const qint64 count = m_stagedWorkbook->write(
                chunk.constData() + written,
                chunk.size() - written);
            if (count <= 0)
            {
                m_stagingError = m_stagedWorkbook->errorString();
                if (m_stagingError.isEmpty())
                {
                    m_stagingError =
                        importText("The downloaded spreadsheet could not be staged.");
                }
                reply->abort();
                return;
            }
            written += count;
        }
    }
}

void NextCalendarEventImportService::handleFinished(QNetworkReply* reply)
{
    if (reply != m_reply)
    {
        reply->deleteLater();
        return;
    }

    handleReadyRead(reply);
    m_reply = nullptr;
    reply->deleteLater();

    if (m_cancelRequested)
    {
        m_stagedWorkbook.reset();
        if (!m_stagedWorkbookPath.isEmpty())
        {
            m_metrics.stagedWorkbookRemovedAtTerminal =
                !QFileInfo::exists(m_stagedWorkbookPath);
        }
        canceled();
        return;
    }
    if (!m_stagingError.isEmpty())
    {
        m_stagedWorkbook.reset();
        fail(m_stagingError);
        return;
    }
    if (reply->error() != QNetworkReply::NoError)
    {
        const QString error = reply->errorString();
        m_stagedWorkbook.reset();
        fail(error.isEmpty()
            ? importText("The Calendar source could not be downloaded.")
            : error);
        return;
    }
    if (!operationIsCurrent())
    {
        m_stagedWorkbook.reset();
        canceled();
        return;
    }
    if (!m_stagedWorkbook || !m_stagedWorkbook->flush()
        || !m_stagedWorkbook->seek(0))
    {
        const QString message = m_stagedWorkbook
            ? m_stagedWorkbook->errorString()
            : importText("The downloaded spreadsheet could not be staged.");
        m_stagedWorkbook.reset();
        fail(message.isEmpty()
            ? importText("The downloaded spreadsheet could not be staged.")
            : message);
        return;
    }

    QString parseError;
    CalendarImport::CalendarWorkbookReadCounts readCounts;
    m_metrics.usedDeviceParser = true;
    CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbookDevice(
            *m_stagedWorkbook,
            m_campusCodeProvider ? m_campusCodeProvider() : QStringList{},
            &parseError,
            &readCounts);
    m_metrics.streamedWorksheetCellCount = readCounts.cellCount;
    m_metrics.materializedWorksheetCellCount =
        readCounts.materializedWorksheetCellCount;
    m_metrics.parsedEventCount = static_cast<int>(parsed.events.size());

    // The parser's input file is removed before any typed signature query or
    // save starts, including the malformed-workbook recovery path.
    m_stagedWorkbook.reset();
    m_metrics.stagedWorkbookReleasedBeforeApply =
        !m_stagedWorkbookPath.isEmpty()
        && !QFileInfo::exists(m_stagedWorkbookPath);
    m_metrics.stagedWorkbookRemovedAtTerminal =
        m_metrics.stagedWorkbookReleasedBeforeApply;

    if (!parseError.isEmpty())
    {
        parsed = {};
        fail(parseError);
        return;
    }
    if (!operationIsCurrent())
    {
        parsed = {};
        canceled();
        return;
    }

    CalendarEventImportUseCaseRequest request;
    request.initiallySkippedCount = parsed.skippedCount;
    if (!parsed.events.isEmpty())
    {
        QDate firstDate = parsed.events.constFirst().startDate;
        QDate lastDate = parsed.events.constFirst().endDate;
        for (const CalendarEvent& event : parsed.events)
        {
            firstDate = qMin(firstDate, event.startDate);
            lastDate = qMax(lastDate, event.endDate);
        }
        request.signatureRange = {
            .startDate = CalendarEventDate(
                firstDate.toString(Qt::ISODate).toStdString()),
            .endDate = CalendarEventDate(
                lastDate.toString(Qt::ISODate).toStdString())
        };
        request.candidates.reserve(static_cast<std::size_t>(parsed.events.size()));
        for (const CalendarEvent& event : parsed.events)
        {
            request.candidates.push_back({
                .signature = CalendarImport::calendarEventImportSignature(event),
                .saveInput = saveInputFor(event)
            });
        }
    }

    const int skippedCount = request.initiallySkippedCount.rawValue();
    {
        QList<CalendarEvent> discardedParserEvents;
        discardedParserEvents.swap(parsed.events);
    }
    m_metrics.legacyEventCountAtApply = static_cast<int>(parsed.events.size());
    m_metrics.typedCandidateCountAtApply =
        static_cast<int>(request.candidates.size());

    if (request.candidates.empty())
    {
        setImporting(false);
        m_metrics.stagedWorkbookRemovedAtTerminal =
            m_metrics.stagedWorkbookReleasedBeforeApply;
        emit importFinished(0, skippedCount);
        return;
    }

    const auto imported = Platform::applyCalendarEventImportAtPath(
        m_databasePath,
        request,
        [this]
        {
            return operationIsCurrent();
        });
    if (!operationIsCurrent())
    {
        canceled();
        return;
    }
    if (!imported)
    {
        fail(text(imported.error().message));
        return;
    }

    setImporting(false);
    emit importFinished(
        imported.value().importedCount.rawValue(),
        imported.value().skippedCount.rawValue());
}

void NextCalendarEventImportService::setImporting(const bool importing)
{
    if (m_importing == importing)
    {
        return;
    }
    m_importing = importing;
    emit importingChanged(importing);
}

void NextCalendarEventImportService::fail(const QString& message)
{
    m_stagedWorkbook.reset();
    if (!m_stagedWorkbookPath.isEmpty())
    {
        m_metrics.stagedWorkbookRemovedAtTerminal =
            !QFileInfo::exists(m_stagedWorkbookPath);
    }
    else
    {
        m_metrics.stagedWorkbookRemovedAtTerminal = true;
    }
    setImporting(false);
    emit importFailed(message.isEmpty()
        ? importText("Calendar import could not be completed.")
        : message);
}

void NextCalendarEventImportService::canceled()
{
    m_stagedWorkbook.reset();
    if (!m_stagedWorkbookPath.isEmpty())
    {
        m_metrics.stagedWorkbookRemovedAtTerminal =
            !QFileInfo::exists(m_stagedWorkbookPath);
    }
    m_cancelRequested = false;
    setImporting(false);
    emit importCanceled();
}

bool NextCalendarEventImportService::operationIsCurrent() const noexcept
{
    if (m_cancelRequested || m_databasePath.isEmpty())
    {
        return false;
    }
    if (!m_activeWorkspacePath)
    {
        return false;
    }
    try
    {
        return m_activeWorkspacePath() == m_databasePath;
    }
    catch (...)
    {
        return false;
    }
}

QString NextCalendarEventImportService::sourceUrl() const
{
    if (!m_sourceUrlOverride.isEmpty())
    {
        return m_sourceUrlOverride;
    }
    return sourceUrlFromEnvironment();
}

} // namespace ClassMngr::Next::Ui

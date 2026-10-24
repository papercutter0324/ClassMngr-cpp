#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/academic_calendar_event_parser.h"
#include "features/calendar/calendar_event_import_service.h"
#include "features/calendar/calendar_workbook_reader.h"

#include <QFile>
#include <QBuffer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QPointer>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

QString workbookFixturePath()
{
    return QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/imports/calendar_import_parity_2026.xlsx");
}

QString legacySignature(const CalendarEvent& event)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(
            event.title.simplified(),
            normalizedCalendarEventType(event.eventType),
            event.startDate.toString(Qt::ISODate),
            event.endDate.toString(Qt::ISODate),
            event.allDay ? QStringLiteral("1") : QStringLiteral("0"),
            normalizedCalendarEventTimeStatus(event.timeStatus)
            );
}

class ScopedCalendarImportUrl final
{
public:
    explicit ScopedCalendarImportUrl(const QByteArray& value)
        : m_hadPreviousValue(qEnvironmentVariableIsSet(
              "CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL"))
        , m_previousValue(qgetenv("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL"))
    {
        qputenv("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL", value);
    }

    ~ScopedCalendarImportUrl()
    {
        if (m_hadPreviousValue)
        {
            qputenv("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL", m_previousValue);
        }
        else
        {
            qunsetenv("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL");
        }
    }

private:
    bool m_hadPreviousValue = false;
    QByteArray m_previousValue;
};

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("calendar-import-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

} // namespace

class NextFeatureCalendarEventImportParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void boundedCalendarParserMatchesGoldenFixture();
    void productionImportPathMatchesLegacyGoldenFixture();
    void failedDownloadCleansStagedWorkbook();
    void failedStagingWriteAbortsAndCleansStagedWorkbook();
    void serviceDestructionCleansInProgressStagedWorkbook();
};

void NextFeatureCalendarEventImportParityTests::
boundedCalendarParserMatchesGoldenFixture()
{
    QFile fixtureFile(workbookFixturePath());
    QVERIFY2(
        fixtureFile.open(QIODevice::ReadOnly),
        qPrintable(QStringLiteral("Required calendar parity fixture is missing: %1")
            .arg(fixtureFile.fileName()))
        );
    const QByteArray workbookBytes = fixtureFile.readAll();
    QVERIFY(!workbookBytes.isEmpty());

    QString workbookError;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(workbookBytes, &workbookError);
    QVERIFY2(workbookError.isEmpty(), qPrintable(workbookError));

    const CalendarImport::ParsedCalendarImport buffered =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QCOMPARE(buffered.events.size(), 4);
    QCOMPARE(buffered.skippedCount.rawValue(), 1);

    QString streamingError;
    CalendarImport::CalendarWorkbookReadCounts readCounts;
    const CalendarImport::ParsedCalendarImport streamed =
        CalendarImport::parseCalendarEventsFromWorkbookBytes(
            workbookBytes,
            {},
            &streamingError,
            &readCounts
            );
    QVERIFY2(streamingError.isEmpty(), qPrintable(streamingError));
    int expectedCellCount = 0;
    int expectedMergedRangeCount = 0;
    for (const CalendarImport::Worksheet& worksheet : workbook.worksheets)
    {
        expectedCellCount += static_cast<int>(worksheet.cells.size());
        expectedMergedRangeCount +=
            static_cast<int>(worksheet.mergedRanges.size());
    }
    QCOMPARE(readCounts.worksheetCount, workbook.worksheets.size());
    QCOMPARE(readCounts.cellCount, expectedCellCount);
    QCOMPARE(readCounts.mergedRangeCount, expectedMergedRangeCount);
    QCOMPARE(readCounts.sharedStringCount, workbook.sharedStrings.size());
    QCOMPARE(readCounts.styleCount, workbook.styles.size());
    QCOMPARE(streamed.events.size(), buffered.events.size());
    QCOMPARE(
        streamed.skippedCount.rawValue(),
        buffered.skippedCount.rawValue()
        );
    for (qsizetype index = 0; index < buffered.events.size(); ++index)
    {
        QCOMPARE(
            legacySignature(streamed.events.at(index)),
            legacySignature(buffered.events.at(index))
            );
    }

    QBuffer device;
    device.setData(workbookBytes);
    QVERIFY(device.open(QIODevice::ReadOnly));
    QString deviceError;
    CalendarImport::CalendarWorkbookReadCounts deviceReadCounts;
    const CalendarImport::ParsedCalendarImport deviceParsed =
        CalendarImport::parseCalendarEventsFromWorkbookDevice(
            device,
            {},
            &deviceError,
            &deviceReadCounts
            );
    QVERIFY2(deviceError.isEmpty(), qPrintable(deviceError));
    QCOMPARE(deviceReadCounts.worksheetCount, readCounts.worksheetCount);
    QCOMPARE(deviceReadCounts.cellCount, readCounts.cellCount);
    QCOMPARE(deviceReadCounts.mergedRangeCount, readCounts.mergedRangeCount);
    QCOMPARE(deviceReadCounts.sharedStringCount, readCounts.sharedStringCount);
    QCOMPARE(deviceReadCounts.styleCount, readCounts.styleCount);
    QCOMPARE(deviceParsed.events.size(), streamed.events.size());
    QCOMPARE(
        deviceParsed.skippedCount.rawValue(),
        streamed.skippedCount.rawValue()
        );
    for (qsizetype index = 0; index < streamed.events.size(); ++index)
    {
        QCOMPARE(
            legacySignature(deviceParsed.events.at(index)),
            legacySignature(streamed.events.at(index))
            );
    }
}

void NextFeatureCalendarEventImportParityTests::
productionImportPathMatchesLegacyGoldenFixture()
{
    // This small golden workbook keeps the parser contract explicit: legacy
    // identity is simplified title, normalized type, ISO date range, all-day
    // bit, and normalized time status. July 2 has two distinct legend matches
    // that resolve to that same key; the legacy parser emits it once.
    QFile fixtureFile(workbookFixturePath());
    QVERIFY2(
        fixtureFile.open(QIODevice::ReadOnly),
        qPrintable(QStringLiteral("Required calendar parity fixture is missing: %1")
            .arg(fixtureFile.fileName()))
        );
    const QByteArray workbookBytes = fixtureFile.readAll();
    QVERIFY(!workbookBytes.isEmpty());

    QString workbookError;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(workbookBytes, &workbookError);
    QVERIFY2(workbookError.isEmpty(), qPrintable(workbookError));
    QCOMPARE(workbook.worksheets.size(), 1);

    const CalendarImport::ParsedCalendarImport parsed =
        CalendarImport::parseCalendarEventsFromWorkbook(workbook);
    QCOMPARE(parsed.events.size(), 4);
    QCOMPARE(parsed.skippedCount.rawValue(), 1);
    const QStringList actualParsedSignatures{
            legacySignature(parsed.events.at(0)),
            legacySignature(parsed.events.at(1)),
            legacySignature(parsed.events.at(2)),
            legacySignature(parsed.events.at(3))
        };
    const QStringList expectedParsedSignatures{
            QStringLiteral("New Semester|Other|2026-07-01|2026-07-01|0|Unknown"),
            QStringLiteral("Board Meeting|CM|2026-07-02|2026-07-02|0|Unknown"),
            QStringLiteral("Red Day|Holiday|2026-07-04|2026-07-04|1|Timed"),
            QStringLiteral("DYB Workshop|Workshop|2026-07-06|2026-07-06|0|Unknown")
        };
    QCOMPARE(actualParsedSignatures, expectedParsedSignatures);

    QString boundedReaderError;
    const CalendarImport::ParsedCalendarImport boundedParse =
        CalendarImport::parseCalendarEventsFromWorkbookBytes(
            workbookBytes,
            {},
            &boundedReaderError
            );
    QVERIFY2(boundedReaderError.isEmpty(), qPrintable(boundedReaderError));
    QCOMPARE(
        boundedParse.skippedCount.rawValue(),
        parsed.skippedCount.rawValue()
        );
    QCOMPARE(boundedParse.events.size(), parsed.events.size());
    for (qsizetype index = 0; index < parsed.events.size(); ++index)
    {
        QCOMPARE(
            legacySignature(boundedParse.events.at(index)),
            expectedParsedSignatures.at(index)
            );
    }

    CalendarImport::ParsedCalendarImport streamedParse;
    bool firstWorksheet = true;
    QString streamError;
    QVERIFY2(
        CalendarImport::forEachWorksheet(
            workbookBytes,
            [&streamedParse, &firstWorksheet](
                const CalendarImport::WorkbookMetadata& metadata,
                const CalendarImport::Worksheet& worksheet
                )
            {
                if (firstWorksheet)
                {
                    streamedParse =
                        CalendarImport::parseCalendarEventsFromWorksheet(
                            metadata,
                            worksheet
                            );
                    firstWorksheet = false;
                }
                return true;
            },
            &streamError
            ),
        qPrintable(streamError)
        );
    QCOMPARE(
        streamedParse.skippedCount.rawValue(),
        parsed.skippedCount.rawValue()
        );
    QCOMPARE(streamedParse.events.size(), parsed.events.size());
    for (qsizetype index = 0; index < parsed.events.size(); ++index)
    {
        QCOMPARE(
            legacySignature(streamedParse.events.at(index)),
            legacySignature(parsed.events.at(index))
            );
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    CalendarService* calendarService = services.calendarService();
    QVERIFY(calendarService);
    QVERIFY(calendarService->isAvailable());

    CalendarEvent preexisting;
    preexisting.title = QStringLiteral("Red Day");
    preexisting.eventType = QStringLiteral("Holiday");
    preexisting.timeStatus = QStringLiteral("Timed");
    preexisting.allDay = true;
    preexisting.startDate = QDate(2026, 7, 4);
    preexisting.endDate = QDate(2026, 7, 4);
    const auto preexistingId = calendarService->saveEvent(preexisting);
    QVERIFY(preexistingId);

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QByteArray receivedRequest;
    bool responseSent = false;
    connect(
        &server,
        &QTcpServer::newConnection,
        &server,
        [&]()
        {
            while (server.hasPendingConnections())
            {
                QTcpSocket* socket = server.nextPendingConnection();
                QVERIFY(socket);
                connect(
                    socket,
                    &QTcpSocket::readyRead,
                    socket,
                    [&, socket]()
                    {
                        receivedRequest.append(socket->readAll());
                        if (responseSent
                            || !receivedRequest.contains("\r\n\r\n"))
                        {
                            return;
                        }

                        responseSent = true;
                        QByteArray response =
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/vnd.openxmlformats-officedocument.spreadsheetml.sheet\r\n"
                            "Connection: close\r\n"
                            "Content-Length: ";
                        response.append(QByteArray::number(workbookBytes.size()));
                        response.append("\r\n\r\n");
                        response.append(workbookBytes);
                        socket->write(response);
                        socket->disconnectFromHost();
                    }
                    );
            }
        }
        );

    const QByteArray fixtureUrl = QStringLiteral("http://127.0.0.1:%1/calendar-fixture.xlsx")
        .arg(server.serverPort())
        .toUtf8();
    ScopedCalendarImportUrl importUrl(fixtureUrl);

    CalendarEventImportService importService(&services);
    QSignalSpy finishedSpy(
        &importService,
        &CalendarEventImportService::importFinished
        );
    QSignalSpy failedSpy(
        &importService,
        &CalendarEventImportService::importFailed
        );
    QVERIFY(finishedSpy.isValid());
    QVERIFY(failedSpy.isValid());

    importService.importFromDefaultSource();
    QVERIFY(importService.isImporting());
    QTRY_VERIFY_WITH_TIMEOUT(responseSent, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 5000);
    QCOMPARE(failedSpy.size(), 0);
    QVERIFY(!importService.isImporting());
    QVERIFY(importService.findChildren<QTemporaryFile*>().isEmpty());
    QVERIFY(receivedRequest.startsWith("GET /calendar-fixture.xlsx HTTP/"));

    const QList<QVariant> importResult = finishedSpy.takeFirst();
    QCOMPARE(importResult.at(0).toInt(), 3);
    // One Weekend cell was ignored by the parser and the existing Red Day
    // matched the imported candidate by the preserved legacy signature.
    QCOMPARE(importResult.at(1).toInt(), 2);

    const auto persisted = calendarService->eventsInRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 6)
        );
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 4);

    QStringList persistedSignatures;
    QList<int> insertedIds;
    for (const CalendarEvent& event : *persisted)
    {
        persistedSignatures.append(legacySignature(event));
        if (event.id != *preexistingId)
        {
            insertedIds.append(event.id);
        }
    }

    const QStringList expectedPersistedSignatures{
            QStringLiteral("New Semester|Other|2026-07-01|2026-07-01|0|Unknown"),
            QStringLiteral("Board Meeting|CM|2026-07-02|2026-07-02|0|Unknown"),
            QStringLiteral("Red Day|Holiday|2026-07-04|2026-07-04|1|Timed"),
            QStringLiteral("DYB Workshop|Workshop|2026-07-06|2026-07-06|0|Unknown")
        };
    QCOMPARE(persistedSignatures, expectedPersistedSignatures);
    QCOMPARE(persisted->at(2).id, *preexistingId);
    QCOMPARE(insertedIds.size(), 3);
    QVERIFY(insertedIds.at(0) < insertedIds.at(1));
    QVERIFY(insertedIds.at(1) < insertedIds.at(2));
    QVERIFY(persisted->at(0).startTime.isNull());
    QVERIFY(persisted->at(0).endTime.isNull());
    QVERIFY(persisted->at(1).startTime.isNull());
    QVERIFY(persisted->at(1).endTime.isNull());
    QVERIFY(persisted->at(3).startTime.isNull());
    QVERIFY(persisted->at(3).endTime.isNull());
    QVERIFY(persisted->at(0).repeatSeriesId.isEmpty());
    QVERIFY(persisted->at(1).repeatSeriesId.isEmpty());
    QVERIFY(persisted->at(3).repeatSeriesId.isEmpty());
}

void NextFeatureCalendarEventImportParityTests::
failedDownloadCleansStagedWorkbook()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    bool responseSent = false;
    QByteArray requestBytes;
    connect(
        &server,
        &QTcpServer::newConnection,
        &server,
        [&]()
        {
            while (server.hasPendingConnections())
            {
                QTcpSocket* socket = server.nextPendingConnection();
                connect(
                    socket,
                    &QTcpSocket::readyRead,
                    socket,
                    [&, socket]()
                    {
                        requestBytes.append(socket->readAll());
                        if (responseSent || !requestBytes.contains("\r\n\r\n"))
                        {
                            return;
                        }
                        responseSent = true;
                        socket->write(
                            "HTTP/1.1 500 Import Failure\r\n"
                            "Content-Length: 5\r\n"
                            "Connection: close\r\n\r\nerror"
                            );
                        socket->disconnectFromHost();
                    }
                    );
            }
        }
        );

    ScopedCalendarImportUrl importUrl(
        QStringLiteral("http://127.0.0.1:%1/network-failure.xlsx")
            .arg(server.serverPort())
            .toUtf8()
        );
    CalendarEventImportService service(&services);
    QSignalSpy finishedSpy(&service, &CalendarEventImportService::importFinished);
    QSignalSpy failedSpy(&service, &CalendarEventImportService::importFailed);
    service.importFromDefaultSource();

    QTRY_VERIFY_WITH_TIMEOUT(responseSent, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(failedSpy.size(), 1, 5000);
    QCOMPARE(finishedSpy.size(), 0);
    QVERIFY(service.findChildren<QTemporaryFile*>().isEmpty());
}

void NextFeatureCalendarEventImportParityTests::
failedStagingWriteAbortsAndCleansStagedWorkbook()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QByteArray body(128, 'x');
    bool initialChunkSent = false;
    QByteArray requestBytes;
    QPointer<QTcpSocket> responseSocket;
    connect(
        &server,
        &QTcpServer::newConnection,
        &server,
        [&]()
        {
            while (server.hasPendingConnections())
            {
                QTcpSocket* socket = server.nextPendingConnection();
                responseSocket = socket;
                connect(
                    socket,
                    &QTcpSocket::readyRead,
                    socket,
                    [&, socket]()
                    {
                        requestBytes.append(socket->readAll());
                        if (
                            initialChunkSent
                            || !requestBytes.contains("\r\n\r\n")
                            )
                        {
                            return;
                        }
                        initialChunkSent = true;
                        QByteArray response =
                            "HTTP/1.1 200 OK\r\nContent-Length: ";
                        response.append(QByteArray::number(body.size()));
                        response.append("\r\nConnection: keep-alive\r\n\r\n");
                        response.append(body.left(2));
                        socket->write(response);
                        socket->flush();
                    }
                    );
            }
        }
        );

    ScopedCalendarImportUrl importUrl(
        QStringLiteral("http://127.0.0.1:%1/write-failure.xlsx")
            .arg(server.serverPort())
            .toUtf8()
        );
    CalendarEventImportService service(&services);
    QSignalSpy failedSpy(&service, &CalendarEventImportService::importFailed);
    service.importFromDefaultSource();

    QTRY_VERIFY_WITH_TIMEOUT(initialChunkSent, 5000);
    QTemporaryFile* stagedWorkbook = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (stagedWorkbook = service.findChild<QTemporaryFile*>())
            && stagedWorkbook->size() == 2,
        5000
        );
    stagedWorkbook->close();
    QVERIFY(responseSocket);
    responseSocket->write(body.mid(2));
    responseSocket->disconnectFromHost();

    QTRY_COMPARE_WITH_TIMEOUT(failedSpy.size(), 1, 5000);
    QVERIFY(service.findChildren<QTemporaryFile*>().isEmpty());
}

void NextFeatureCalendarEventImportParityTests::
serviceDestructionCleansInProgressStagedWorkbook()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    bool partialResponseSent = false;
    QByteArray requestBytes;
    connect(
        &server,
        &QTcpServer::newConnection,
        &server,
        [&]()
        {
            while (server.hasPendingConnections())
            {
                QTcpSocket* socket = server.nextPendingConnection();
                connect(
                    socket,
                    &QTcpSocket::readyRead,
                    socket,
                    [&, socket]()
                    {
                        requestBytes.append(socket->readAll());
                        if (
                            partialResponseSent
                            || !requestBytes.contains("\r\n\r\n")
                            )
                        {
                            return;
                        }
                        partialResponseSent = true;
                        socket->write(
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Length: 1000\r\n"
                            "Connection: keep-alive\r\n\r\npartial"
                            );
                        socket->flush();
                    }
                    );
            }
        }
        );

    ScopedCalendarImportUrl importUrl(
        QStringLiteral("http://127.0.0.1:%1/abandoned.xlsx")
            .arg(server.serverPort())
            .toUtf8()
        );
    auto* service = new CalendarEventImportService(&services);
    service->importFromDefaultSource();

    QTRY_VERIFY_WITH_TIMEOUT(partialResponseSent, 5000);
    QTemporaryFile* stagedWorkbook = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (stagedWorkbook = service->findChild<QTemporaryFile*>())
            && stagedWorkbook->size() > 0,
        5000
        );
    const QString stagedPath = stagedWorkbook->fileName();
    QVERIFY(QFile::exists(stagedPath));

    delete service;
    QVERIFY(!QFile::exists(stagedPath));
}

QTEST_GUILESS_MAIN(NextFeatureCalendarEventImportParityTests)

#include "next_feature_calendar_event_import_parity_tests.moc"

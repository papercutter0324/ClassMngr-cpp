#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/academic_calendar_event_parser.h"
#include "features/calendar/calendar_event_import_service.h"
#include "features/calendar/calendar_workbook_reader.h"

#include <QFile>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
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
    void productionImportPathMatchesLegacyGoldenFixture();
};

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
    QCOMPARE(parsed.skippedCount, 1);
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

QTEST_GUILESS_MAIN(NextFeatureCalendarEventImportParityTests)

#include "next_feature_calendar_event_import_parity_tests.moc"

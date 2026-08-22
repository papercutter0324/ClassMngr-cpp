#include "data/repositories/calendar_event_repository.h"
#include "features/calendar/ui/calendar_event_cache.h"
#include "features/calendar/ui/calendar_event_model.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <algorithm>

namespace
{
void createCalendarEventsTable(
    QSqlDatabase& database
    )
{
    QSqlQuery query(database);

    QVERIFY(
        query.exec(R"(
            CREATE TABLE calendar_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                event_type TEXT DEFAULT 'Other',
                time_status TEXT DEFAULT 'Timed',
                repeat_series_id TEXT,
                all_day INTEGER DEFAULT 0,
                start_date TEXT,
                start_time TEXT,
                end_date TEXT,
                end_time TEXT
            )
        )")
        );
}

void saveEvent(
    QSqlDatabase& database,
    const QDate& date,
    const QString& title,
    const QDate& endDate = {}
    )
{
    CalendarEvent event;
    event.title = title;
    event.startDate = date;
    event.endDate = endDate.isValid()
        ? endDate
        : date;
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);

    CalendarEventRepository repository(database);
    const Result<int> saved = repository.saveCalendarEvent(event);
    QVERIFY(saved);
    QVERIFY(*saved > 0);
}

void createDatabase(
    const QString& databasePath
    )
{
    const QString connectionName =
        QStringLiteral("calendar-event-cache-setup-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            );

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        createCalendarEventsTable(database);
        saveEvent(
            database,
            QDate(2026, 7, 10),
            QStringLiteral("Cached event")
            );
        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}
}

class CalendarEventCacheTests : public QObject
{
    Q_OBJECT

private slots:
    void rangeLoadPopulatesModelWithoutUiThreadDatabaseAccess();
    void invalidationDiscardsCompletedWorkerResult();
    void multiDayEventsUseOneCanonicalRecordAndRangeDeduplicates();
    void retainedRangesEvictEventsAndRejectEvictedWorkerResults();
};

void CalendarEventCacheTests::rangeLoadPopulatesModelWithoutUiThreadDatabaseAccess()
{
    MemoryUsageDiagnostics::enable();
    MemoryUsageDiagnostics::history().clear();
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    CalendarEventCache cache;
    CalendarEventModel model(&cache);
    cache.setDatabasePath(databasePath);
    cache.requestRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );

    QTRY_VERIFY_WITH_TIMEOUT(
        cache.isRangeLoaded(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            ),
        5000
        );

    const QVariantList events =
        model.eventsForDate(2026, 7, 10);

    QCOMPARE(events.size(), 1);
    QCOMPARE(
        events.first().toMap().value(QStringLiteral("title")).toString(),
        QStringLiteral("Cached event")
        );
    QVERIFY(model.isMonthLoaded(2026, 7));
    const QList<MemoryUsageHistoryEntry>& diagnosticEvents =
        MemoryUsageDiagnostics::history().entries();
    QVERIFY(std::any_of(
        diagnosticEvents.cbegin(),
        diagnosticEvents.cend(),
        [](const MemoryUsageHistoryEntry& entry)
        {
            return entry.kind == MemoryUsageHistoryEntryKind::Event
                && entry.eventType == QStringLiteral("timing")
                && entry.eventDetail.contains(
                    QStringLiteral("calendar-cache-fetch")
                    );
        }
        ));
}

void CalendarEventCacheTests::invalidationDiscardsCompletedWorkerResult()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    CalendarEventCache cache;
    cache.setDatabasePath(databasePath);
    cache.requestRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );
    cache.invalidate();

    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QVERIFY(
        !cache.isRangeLoaded(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            )
        );
    QVERIFY(cache.eventsForDate(QDate(2026, 7, 10)).isEmpty());

    cache.requestRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        cache.isRangeLoaded(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            ),
        5000
        );
    QCOMPARE(cache.eventsForDate(QDate(2026, 7, 10)).size(), 1);
}

void CalendarEventCacheTests::multiDayEventsUseOneCanonicalRecordAndRangeDeduplicates()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    const QString connectionName =
        QStringLiteral("calendar-event-cache-multiday-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            );
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        saveEvent(
            database,
            QDate(2026, 7, 12),
            QStringLiteral("Three-day event"),
            QDate(2026, 7, 14)
            );
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    CalendarEventCache cache;
    cache.setDatabasePath(databasePath);
    cache.requestRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        cache.isRangeLoaded(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            ),
        5000
        );

    QCOMPARE(cache.eventCount(), 2);
    QCOMPARE(cache.dateBucketCount(), 4);
    const MemoryBreakdownEntry attribution =
        cache.memoryBreakdown().constFirst();
    QCOMPARE(attribution.owner, QStringLiteral("Calendar"));
    QCOMPARE(attribution.itemCount, quint64(6));
    QVERIFY(attribution.retainedBytes > 0);
    QVERIFY(attribution.isEstimated);
    QCOMPARE(cache.eventsForDate(QDate(2026, 7, 12)).size(), 1);
    QCOMPARE(cache.eventsForDate(QDate(2026, 7, 13)).size(), 1);
    QCOMPARE(
        cache.eventsInRange(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            ).size(),
        2
        );
}

void CalendarEventCacheTests::retainedRangesEvictEventsAndRejectEvictedWorkerResults()
{
    MemoryUsageDiagnostics::enable();
    MemoryUsageDiagnostics::history().clear();
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    CalendarEventCache cache;
    cache.setDatabasePath(databasePath);
    cache.setRetainedRanges(
        {
            {
                QDate(2026, 7, 1),
                QDate(2026, 7, 31)
            }
        }
        );
    cache.requestRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        cache.isRangeLoaded(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            ),
        5000
        );
    QCOMPARE(cache.eventCount(), 1);

    cache.setRetainedRanges(
        {
            {
                QDate(2026, 8, 1),
                QDate(2026, 8, 31)
            }
        }
        );
    QVERIFY(
        !cache.isRangeLoaded(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            )
        );
    QCOMPARE(cache.eventCount(), 0);
    QCOMPARE(cache.dateBucketCount(), 0);
    const QList<MemoryUsageHistoryEntry>& diagnosticEvents =
        MemoryUsageDiagnostics::history().entries();
    QVERIFY(std::any_of(
        diagnosticEvents.cbegin(),
        diagnosticEvents.cend(),
        [](const MemoryUsageHistoryEntry& entry)
        {
            return entry.kind == MemoryUsageHistoryEntryKind::Event
                && entry.eventType == QStringLiteral("calendar-cache-evicted")
                && entry.eventDetail.contains(QStringLiteral("events=1"));
        }
        ));

    cache.setRetainedRanges(
        {
            {
                QDate(2026, 7, 1),
                QDate(2026, 7, 31)
            }
        }
        );
    cache.requestRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );
    cache.setRetainedRanges(
        {
            {
                QDate(2026, 8, 1),
                QDate(2026, 8, 31)
            }
        }
        );
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QCOMPARE(cache.eventCount(), 0);
    QCOMPARE(cache.dateBucketCount(), 0);
}

QTEST_GUILESS_MAIN(CalendarEventCacheTests)

#include "calendar_event_cache_tests.moc"

#include "classmngr/engine/calendar_event_service.h"
#include "classmngr/engine/open_database.h"
#include "features/calendar/ui/calendar_event_cache.h"
#include "features/calendar/ui/calendar_event_model.h"

#include <QFileInfo>
#include <QByteArray>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string_view>

namespace
{
void saveEvent(
    classmngr::engine::CalendarEventService& service,
    const QDate& date,
    const QString& title,
    const QDate& endDate = {}
    )
{
    classmngr::engine::CalendarEvent event;
    const QByteArray utf8Title = title.toUtf8();
    event.title = std::string(
        utf8Title.constData(),
        static_cast<std::size_t>(utf8Title.size())
        );
    event.startDate = calendar_event_detail::toEngineDate(date);
    event.endDate = calendar_event_detail::toEngineDate(
        endDate.isValid() ? endDate : date
        );
    event.startTime = std::chrono::hours{9};
    event.endTime = std::chrono::hours{10};

    const classmngr::engine::Result<int> saved = service.save(event);
    QVERIFY(saved);
    QVERIFY(*saved > 0);
}

void createDatabase(
    const QString& databasePath
    )
{
    const QByteArray utf8Path = databasePath.toUtf8();
    const auto database = classmngr::engine::OpenDatabase::execute(
        std::string_view(
            utf8Path.constData(),
            static_cast<std::size_t>(utf8Path.size())
            )
        );
    QVERIFY(database);

    classmngr::engine::CalendarEventService service(**database);
    saveEvent(
        service,
        QDate(2026, 7, 10),
        QStringLiteral("Cached event")
        );
}

void appendEvent(
    const QString& databasePath,
    const QDate& date,
    const QString& title,
    const QDate& endDate = {}
    )
{
    const QByteArray utf8Path = databasePath.toUtf8();
    const auto database = classmngr::engine::OpenDatabase::execute(
        std::string_view(
            utf8Path.constData(),
            static_cast<std::size_t>(utf8Path.size())
            )
        );
    QVERIFY(database);

    classmngr::engine::CalendarEventService service(**database);
    saveEvent(service, date, title, endDate);
}
}

class CalendarEventCacheTests : public QObject
{
    Q_OBJECT

private slots:
    void rangeLoadPopulatesModelWithoutUiThreadDatabaseAccess();
    void fileBackedLoadPreflightsAndCreatesUnicodePath();
    void nextEventMonthUsesEngineForFileBackedDatabase();
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

void CalendarEventCacheTests::fileBackedLoadPreflightsAndCreatesUnicodePath()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("프로필/캘린더.db")
        );

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
    QVERIFY(QFileInfo::exists(databasePath));
    QVERIFY(QFileInfo(databasePath).isFile());
    QVERIFY(cache.eventsInRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        ).isEmpty());
}

void CalendarEventCacheTests::nextEventMonthUsesEngineForFileBackedDatabase()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);
    appendEvent(
        databasePath,
        QDate(2026, 7, 20),
        QStringLiteral("Next cached event")
        );

    CalendarEventCache cache;
    QSignalSpy nextEventSpy(
        &cache,
        &CalendarEventCache::nextEventMonthFound
        );
    cache.setDatabasePath(databasePath);
    cache.requestNextEventMonth(QDate(2026, 7, 11));

    QTRY_COMPARE_WITH_TIMEOUT(nextEventSpy.count(), 1, 5000);
    QCOMPARE(
        nextEventSpy.constFirst().constFirst().toDate(),
        QDate(2026, 7, 20)
        );
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

    appendEvent(
        databasePath,
        QDate(2026, 7, 12),
        QStringLiteral("Three-day event"),
        QDate(2026, 7, 14)
        );

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

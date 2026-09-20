#include "data/repositories/calendar_event_repository.h"
#include "features/calendar/calendar_event_projection_query.h"
#include "features/calendar/ui/calendar_event_cache.h"
#include "features/calendar/ui/calendar_event_model.h"

#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

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

void insertRawEvent(
    QSqlDatabase& database,
    const QString& title,
    const QString& eventType,
    const QString& timeStatus,
    const QString& repeatSeriesId,
    bool allDay,
    const QDate& startDate,
    const QString& startTime,
    const QDate& endDate,
    const QString& endTime
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO calendar_events (
            title,
            event_type,
            time_status,
            repeat_series_id,
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(title);
    query.addBindValue(eventType);
    query.addBindValue(timeStatus);
    query.addBindValue(
        repeatSeriesId.isEmpty()
            ? QVariant()
            : QVariant(repeatSeriesId)
        );
    query.addBindValue(allDay ? 1 : 0);
    query.addBindValue(startDate.toString(Qt::ISODate));
    query.addBindValue(
        startTime.isEmpty()
            ? QVariant()
            : QVariant(startTime)
        );
    query.addBindValue(endDate.toString(Qt::ISODate));
    query.addBindValue(
        endTime.isEmpty()
            ? QVariant()
            : QVariant(endTime)
        );
    QVERIFY(query.exec());
}

QSet<QString> connectionNameSet()
{
    QSet<QString> names;
    for (const QString& name : QSqlDatabase::connectionNames())
    {
        names.insert(name);
    }
    return names;
}

QString projectionText(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
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
    void projectionMappingPreservesRichCalendarFieldsAndCleansConnections();
    void nextEventLookupUsesProjectionQueryAndDeduplicates();
    void invalidationDiscardsCompletedWorkerResult();
    void multiDayEventsUseOneCanonicalRecordAndRangeDeduplicates();
    void retainedRangesEvictEventsAndRejectEvictedWorkerResults();
};

void CalendarEventCacheTests::rangeLoadPopulatesModelWithoutUiThreadDatabaseAccess()
{
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
}

void CalendarEventCacheTests::projectionMappingPreservesRichCalendarFieldsAndCleansConnections()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    const QString connectionName =
        QStringLiteral("calendar-event-cache-projection-fixture-%1").arg(
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
        insertRawEvent(
            database,
            QStringLiteral("Repeat unknown-time"),
            QStringLiteral("Holiday"),
            QStringLiteral("Unknown"),
            QStringLiteral("  series-1  "),
            false,
            QDate(2026, 7, 11),
            QString(),
            QDate(2026, 7, 11),
            QString()
            );
        insertRawEvent(
            database,
            QStringLiteral("All-day event"),
            QStringLiteral("Vacation"),
            QStringLiteral("Timed"),
            QString(),
            true,
            QDate(2026, 7, 12),
            QStringLiteral("13:00"),
            QDate(2026, 7, 12),
            QStringLiteral("14:00")
            );
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const QSet<QString> connectionsBeforeQuery = connectionNameSet();
    const auto projection = CalendarEventProjectionQuery::loadRange(
        databasePath,
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );
    QVERIFY(projection);
    QCOMPARE(projection.value().eventCount(), std::size_t(3));

    const auto findSummary =
        [&projection](const QString& title)
        -> std::optional<
            ClassMngr::Next::Application::CalendarEventSummary>
        {
            for (const auto& summary : projection.value().events())
            {
                if (projectionText(summary.title) == title)
                {
                    return summary;
                }
            }
            return std::nullopt;
        };

    const auto repeat = findSummary(
        QStringLiteral("Repeat unknown-time")
        );
    QVERIFY(repeat);
    QCOMPARE(repeat->eventType, std::string("Holiday"));
    QCOMPARE(repeat->timeStatus, std::string("Unknown"));
    QCOMPARE(
        repeat->repeatSeriesId,
        std::optional<std::string>(std::string("series-1"))
        );
    QVERIFY(!repeat->startTime);
    QVERIFY(!repeat->endTime);

    const auto allDay = findSummary(QStringLiteral("All-day event"));
    QVERIFY(allDay);
    QVERIFY(allDay->allDay);
    QVERIFY(!allDay->startTime);
    QVERIFY(!allDay->endTime);
    QCOMPARE(connectionNameSet(), connectionsBeforeQuery);

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

    const QList<CalendarEvent> repeatEvents =
        cache.eventsForDate(QDate(2026, 7, 11));
    QCOMPARE(repeatEvents.size(), 1);
    QCOMPARE(repeatEvents.first().title, QStringLiteral("Repeat unknown-time"));
    QCOMPARE(repeatEvents.first().eventType, QStringLiteral("Holiday"));
    QCOMPARE(repeatEvents.first().timeStatus, QStringLiteral("Unknown"));
    QCOMPARE(repeatEvents.first().repeatSeriesId, QStringLiteral("series-1"));
    QVERIFY(!repeatEvents.first().startTime.isValid());
    QVERIFY(!repeatEvents.first().endTime.isValid());

    const QList<CalendarEvent> allDayEvents =
        cache.eventsForDate(QDate(2026, 7, 12));
    QCOMPARE(allDayEvents.size(), 1);
    QVERIFY(allDayEvents.first().allDay);
    QVERIFY(!allDayEvents.first().startTime.isValid());
    QVERIFY(!allDayEvents.first().endTime.isValid());
    QCOMPARE(connectionNameSet(), connectionsBeforeQuery);
}

void CalendarEventCacheTests::nextEventLookupUsesProjectionQueryAndDeduplicates()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    const QSet<QString> connectionsBeforeQuery = connectionNameSet();
    const auto directNextDate = CalendarEventProjectionQuery::findNextEventDate(
        databasePath,
        QDate(2026, 7, 1)
        );
    QVERIFY(directNextDate);
    QCOMPARE(directNextDate.value(), QDate(2026, 7, 10));
    QVERIFY(
        !CalendarEventProjectionQuery::findNextEventDate(
            databasePath,
            QDate(2026, 7, 31)
            ).value().isValid()
        );
    QCOMPARE(connectionNameSet(), connectionsBeforeQuery);

    CalendarEventCache cache;
    cache.setDatabasePath(databasePath);
    QSignalSpy foundSpy(
        &cache,
        &CalendarEventCache::nextEventMonthFound
        );

    cache.requestNextEventMonth(QDate(2026, 7, 1));
    cache.requestNextEventMonth(QDate(2026, 7, 1));
    QTRY_COMPARE_WITH_TIMEOUT(foundSpy.count(), 1, 5000);
    QCOMPARE(foundSpy.at(0).at(0).toDate(), QDate(2026, 7, 10));

    cache.requestNextEventMonth(QDate(2026, 7, 31));
    QTRY_COMPARE_WITH_TIMEOUT(foundSpy.count(), 2, 5000);
    QVERIFY(!foundSpy.at(1).at(0).toDate().isValid());
    QCOMPARE(connectionNameSet(), connectionsBeforeQuery);
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

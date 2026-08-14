#include "data/repositories/calendar_event_repository.h"
#include "features/calendar/ui/calendar_event_cache.h"
#include "features/calendar/ui/calendar_event_model.h"

#include <QSqlDatabase>
#include <QSqlQuery>
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
    const QString& title
    )
{
    CalendarEvent event;
    event.title = title;
    event.startDate = date;
    event.endDate = date;
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

QTEST_GUILESS_MAIN(CalendarEventCacheTests)

#include "calendar_event_cache_tests.moc"

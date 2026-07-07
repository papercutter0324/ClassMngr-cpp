#include "data/repositories/calendar_event_repository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
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

CalendarEvent makeEvent(
    const QString& title,
    const QDate& startDate,
    const QTime& startTime,
    const QDate& endDate,
    const QTime& endTime,
    const QString& eventType = QStringLiteral("Other"),
    const QString& repeatSeriesId = QString()
    )
{
    CalendarEvent calendarEvent;
    calendarEvent.title = title;
    calendarEvent.eventType = eventType;
    calendarEvent.repeatSeriesId = repeatSeriesId;
    calendarEvent.startDate = startDate;
    calendarEvent.startTime = startTime;
    calendarEvent.endDate = endDate;
    calendarEvent.endTime = endTime;
    return calendarEvent;
}

QStringList titles(
    const QList<CalendarEvent>& events
    )
{
    QStringList values;

    for (const CalendarEvent& event : events)
    {
        values.append(event.title);
    }

    return values;
}
}

class CalendarEventRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void rangeQueryIncludesEventsThatOverlapRange();
    void rangeQuerySortsByDateTimeAndTitle();
    void upcomingQueryExcludesPastEventsAndLimitsResults();
    void savesAndLoadsRepeatSeriesId();
    void repeatSeriesQueryLoadsSelectedAndFollowingOnly();
    void repeatSeriesDeleteRemovesSelectedAndFollowingOnly();
};

void CalendarEventRepositoryTests::rangeQueryIncludesEventsThatOverlapRange()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_range_tests");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            QStringLiteral(":memory:")
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Inside"),
                QDate(2026, 7, 10),
                QTime(9, 0),
                QDate(2026, 7, 10),
                QTime(10, 0)
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Overlaps Start"),
                QDate(2026, 6, 30),
                QTime(9, 0),
                QDate(2026, 7, 2),
                QTime(10, 0)
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Overlaps End"),
                QDate(2026, 7, 31),
                QTime(9, 0),
                QDate(2026, 8, 2),
                QTime(10, 0)
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Outside"),
                QDate(2026, 8, 3),
                QTime(9, 0),
                QDate(2026, 8, 3),
                QTime(10, 0)
                )
            );

        QCOMPARE(
            titles(
                repository.loadCalendarEventsInRange(
                    QDate(2026, 7, 1),
                    QDate(2026, 7, 31)
                    )
                ),
            QStringList({
                QStringLiteral("Overlaps Start"),
                QStringLiteral("Inside"),
                QStringLiteral("Overlaps End")
            })
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::rangeQuerySortsByDateTimeAndTitle()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_sort_tests");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            QStringLiteral(":memory:")
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Later Date"),
                QDate(2026, 7, 11),
                QTime(8, 0),
                QDate(2026, 7, 11),
                QTime(9, 0)
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Beta"),
                QDate(2026, 7, 10),
                QTime(9, 0),
                QDate(2026, 7, 10),
                QTime(10, 0)
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Alpha"),
                QDate(2026, 7, 10),
                QTime(9, 0),
                QDate(2026, 7, 10),
                QTime(10, 0)
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Early Time"),
                QDate(2026, 7, 10),
                QTime(8, 0),
                QDate(2026, 7, 10),
                QTime(9, 0)
                )
            );

        QCOMPARE(
            titles(
                repository.loadCalendarEventsInRange(
                    QDate(2026, 7, 1),
                    QDate(2026, 7, 31)
                    )
                ),
            QStringList({
                QStringLiteral("Early Time"),
                QStringLiteral("Alpha"),
                QStringLiteral("Beta"),
                QStringLiteral("Later Date")
            })
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::upcomingQueryExcludesPastEventsAndLimitsResults()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_upcoming_tests");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            QStringLiteral(":memory:")
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Past"),
                QDate(2026, 6, 1),
                QTime(9, 0),
                QDate(2026, 6, 2),
                QTime(10, 0)
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Ongoing"),
                QDate(2026, 6, 30),
                QTime(9, 0),
                QDate(2026, 7, 5),
                QTime(10, 0)
                )
            );

        for (int index = 1; index <= 12; ++index)
        {
            repository.saveCalendarEvent(
                makeEvent(
                    QStringLiteral("Future %1").arg(index),
                    QDate(2026, 7, 5).addDays(index),
                    QTime(9, 0),
                    QDate(2026, 7, 5).addDays(index),
                    QTime(10, 0)
                    )
                );
        }

        const QList<CalendarEvent> upcoming =
            repository.loadUpcomingCalendarEvents(
                QDate(2026, 7, 5),
                10
                );

        QCOMPARE(upcoming.size(), 10);
        QVERIFY(!titles(upcoming).contains(QStringLiteral("Past")));
        QCOMPARE(upcoming.first().title, QStringLiteral("Ongoing"));
        QCOMPARE(upcoming.last().title, QStringLiteral("Future 9"));
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::savesAndLoadsRepeatSeriesId()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_series_save_tests");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            QStringLiteral(":memory:")
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        const int eventId =
            repository.saveCalendarEvent(
                makeEvent(
                    QStringLiteral("Series Event"),
                    QDate(2026, 7, 10),
                    QTime(9, 0),
                    QDate(2026, 7, 10),
                    QTime(10, 0),
                    QStringLiteral("Meeting"),
                    QStringLiteral("series-1")
                    )
                );

        const CalendarEvent loaded =
            repository.getCalendarEvent(eventId);

        QCOMPARE(loaded.repeatSeriesId, QStringLiteral("series-1"));

        CalendarEvent detached =
            loaded;
        detached.repeatSeriesId.clear();
        repository.saveCalendarEvent(detached);

        QCOMPARE(
            repository.getCalendarEvent(eventId).repeatSeriesId,
            QString()
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::repeatSeriesQueryLoadsSelectedAndFollowingOnly()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_series_query_tests");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            QStringLiteral(":memory:")
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Series 1"),
                QDate(2026, 7, 1),
                QTime(9, 0),
                QDate(2026, 7, 1),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-1")
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Series 2"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-1")
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Series 3"),
                QDate(2026, 7, 15),
                QTime(9, 0),
                QDate(2026, 7, 15),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-1")
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Other Series"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-2")
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Standalone"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0)
                )
            );

        QCOMPARE(
            titles(
                repository.loadCalendarEventsForRepeatSeriesFromDate(
                    QStringLiteral("series-1"),
                    QDate(2026, 7, 8)
                    )
                ),
            QStringList({
                QStringLiteral("Series 2"),
                QStringLiteral("Series 3")
            })
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::repeatSeriesDeleteRemovesSelectedAndFollowingOnly()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_series_delete_tests");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            QStringLiteral(":memory:")
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Series 1"),
                QDate(2026, 7, 1),
                QTime(9, 0),
                QDate(2026, 7, 1),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-1")
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Series 2"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-1")
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Series 3"),
                QDate(2026, 7, 15),
                QTime(9, 0),
                QDate(2026, 7, 15),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-1")
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Other Series"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-2")
                )
            );
        repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Standalone"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0)
                )
            );

        repository.deleteCalendarEventsForRepeatSeriesFromDate(
            QStringLiteral("series-1"),
            QDate(2026, 7, 8)
            );

        QCOMPARE(
            titles(
                repository.loadCalendarEventsInRange(
                    QDate(2026, 7, 1),
                    QDate(2026, 7, 31)
                    )
                ),
            QStringList({
                QStringLiteral("Series 1"),
                QStringLiteral("Other Series"),
                QStringLiteral("Standalone")
            })
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CalendarEventRepositoryTests)

#include "calendar_event_repository_tests.moc"

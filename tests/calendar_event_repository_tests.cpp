#include "data/repositories/calendar_event_repository.h"
#include "data/database/database_schema_manager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
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

void saveCalendarEventOrFail(
    CalendarEventRepository& repository,
    const CalendarEvent& calendarEvent
    )
{
    QVERIFY(repository.saveCalendarEvent(calendarEvent).has_value());
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
    void nextEventQueryFindsEarliestFutureStartDate();
    void schemaCreatesEndDateIndex();
    void savesAndLoadsRepeatSeriesId();
    void repeatSeriesQueryLoadsSelectedAndFollowingOnly();
    void repeatSeriesDeleteRemovesSelectedAndFollowingOnly();
    void writeFailuresAreReturnedAndBatchRollsBack();
};

void CalendarEventRepositoryTests::rangeQueryIncludesEventsThatOverlapRange()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_range_tests");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("events.db"))
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Inside"),
                QDate(2026, 7, 10),
                QTime(9, 0),
                QDate(2026, 7, 10),
                QTime(10, 0)
                )
            );
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Overlaps Start"),
                QDate(2026, 6, 30),
                QTime(9, 0),
                QDate(2026, 7, 2),
                QTime(10, 0)
                )
            );
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Overlaps End"),
                QDate(2026, 7, 31),
                QTime(9, 0),
                QDate(2026, 8, 2),
                QTime(10, 0)
                )
            );
        saveCalendarEventOrFail(repository,
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
                    ).value_or(QList<CalendarEvent>{})
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
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("events.db"))
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Later Date"),
                QDate(2026, 7, 11),
                QTime(8, 0),
                QDate(2026, 7, 11),
                QTime(9, 0)
                )
            );
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Beta"),
                QDate(2026, 7, 10),
                QTime(9, 0),
                QDate(2026, 7, 10),
                QTime(10, 0)
                )
            );
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Alpha"),
                QDate(2026, 7, 10),
                QTime(9, 0),
                QDate(2026, 7, 10),
                QTime(10, 0)
                )
            );
        saveCalendarEventOrFail(repository,
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
                    ).value_or(QList<CalendarEvent>{})
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
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("events.db"))
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Past"),
                QDate(2026, 6, 1),
                QTime(9, 0),
                QDate(2026, 6, 2),
                QTime(10, 0)
                )
            );
        saveCalendarEventOrFail(repository,
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
            saveCalendarEventOrFail(repository,
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
                ).value_or(QList<CalendarEvent>{});

        QCOMPARE(upcoming.size(), 10);
        QVERIFY(!titles(upcoming).contains(QStringLiteral("Past")));
        QCOMPARE(upcoming.first().title, QStringLiteral("Ongoing"));
        QCOMPARE(upcoming.last().title, QStringLiteral("Future 9"));
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::nextEventQueryFindsEarliestFutureStartDate()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_next_event_tests");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("events.db"))
            );
        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Earlier"),
                QDate(2026, 7, 5),
                QTime(9, 0),
                QDate(2026, 7, 5),
                QTime(10, 0)
                )
            );
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Later"),
                QDate(2026, 8, 12),
                QTime(9, 0),
                QDate(2026, 8, 12),
                QTime(10, 0)
                )
            );

        const Result<QDate> nextEventDate =
            repository.findNextCalendarEventStartDate(QDate(2026, 7, 6));
        QVERIFY(nextEventDate);
        QCOMPARE(*nextEventDate, QDate(2026, 8, 12));

        const Result<QDate> noNextEventDate =
            repository.findNextCalendarEventStartDate(QDate(2026, 9, 1));
        QVERIFY(noNextEventDate);
        QVERIFY(!noNextEventDate->isValid());
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::schemaCreatesEndDateIndex()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_schema_index_tests");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA index_list(calendar_events)")));

        bool foundEndDateIndex = false;
        while (query.next())
        {
            if (
                query.value(QStringLiteral("name")).toString()
                == QStringLiteral("idx_calendar_events_end_dates")
                )
            {
                foundEndDateIndex = true;
                break;
            }
        }

        QVERIFY(foundEndDateIndex);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::savesAndLoadsRepeatSeriesId()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_series_save_tests");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("events.db"))
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        const Result<int> savedEvent =
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
        QVERIFY(savedEvent);
        const int eventId = *savedEvent;

        const Result<CalendarEvent> loaded =
            repository.getCalendarEvent(eventId);
        QVERIFY(loaded);

        QCOMPARE(loaded->repeatSeriesId, QStringLiteral("series-1"));

        CalendarEvent detached =
            *loaded;
        detached.repeatSeriesId.clear();
        saveCalendarEventOrFail(repository, detached);

        QCOMPARE(
            repository.getCalendarEvent(eventId)->repeatSeriesId,
            QString()
            );
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::repeatSeriesQueryLoadsSelectedAndFollowingOnly()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_series_query_tests");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("events.db"))
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        saveCalendarEventOrFail(repository,
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
        saveCalendarEventOrFail(repository,
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
        saveCalendarEventOrFail(repository,
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
        saveCalendarEventOrFail(repository,
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
        saveCalendarEventOrFail(repository,
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
                    ).value_or(QList<CalendarEvent>{})
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
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("events.db"))
            );

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        saveCalendarEventOrFail(repository,
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
        saveCalendarEventOrFail(repository,
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
        saveCalendarEventOrFail(repository,
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
        saveCalendarEventOrFail(repository,
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
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Standalone"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0)
                )
            );

        QVERIFY(repository.deleteCalendarEventsForRepeatSeriesFromDate(
            QStringLiteral("series-1"),
            QDate(2026, 7, 8)
            ).has_value());

        QCOMPARE(
            titles(
                repository.loadCalendarEventsInRange(
                    QDate(2026, 7, 1),
                    QDate(2026, 7, 31)
                    ).value_or(QList<CalendarEvent>{})
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

void CalendarEventRepositoryTests::writeFailuresAreReturnedAndBatchRollsBack()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_failure_tests");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("events.db"))
            );
        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        QVERIFY(repository.loadCalendarEventsInRange(
            QDate(2026, 8, 1),
            QDate(2026, 8, 31)
            ).has_value());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER reject_calendar_insert "
            "BEFORE INSERT ON calendar_events "
            "WHEN NEW.title = 'Reject' "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected calendar insert failure'); "
            "END"
            )));

        const Result<QList<int>> batchSaved = repository.saveCalendarEvents({
            makeEvent(
                QStringLiteral("Must Roll Back"),
                QDate(2026, 8, 1),
                QTime(9, 0),
                QDate(2026, 8, 1),
                QTime(10, 0)
                ),
            makeEvent(
                QStringLiteral("Reject"),
                QDate(2026, 8, 2),
                QTime(9, 0),
                QDate(2026, 8, 2),
                QTime(10, 0)
                )
        });
        QVERIFY(!batchSaved);
        QVERIFY(batchSaved.error().contains(
            QStringLiteral("Creating calendar event")
            ));
        QVERIFY(batchSaved.error().contains(QStringLiteral("Reject")));
        QVERIFY(repository.loadCalendarEventsInRange(
            QDate(2026, 8, 1),
            QDate(2026, 8, 31)
            )->isEmpty());

        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_calendar_insert")));
        CalendarEvent existing = makeEvent(
            QStringLiteral("Existing"),
            QDate(2026, 8, 3),
            QTime(9, 0),
            QDate(2026, 8, 3),
            QTime(10, 0),
            QStringLiteral("Other"),
            QStringLiteral("failure-series")
            );
        const Result<int> created = repository.saveCalendarEvent(existing);
        QVERIFY(created);
        existing.id = *created;
        existing.title = QStringLiteral("Changed");

        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER reject_calendar_update "
            "BEFORE UPDATE ON calendar_events "
            "WHEN OLD.id = %1 "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected calendar update failure'); "
            "END"
            ).arg(existing.id)));
        const Result<int> updated = repository.saveCalendarEvent(existing);
        QVERIFY(!updated);
        QVERIFY(updated.error().contains(
            QStringLiteral("Updating calendar event")
            ));
        QVERIFY(updated.error().contains(
            QStringLiteral("calendar event id %1").arg(existing.id)
            ));
        QCOMPARE(repository.getCalendarEvent(existing.id)->title,
                 QStringLiteral("Existing"));

        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_calendar_update")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER reject_calendar_delete "
            "BEFORE DELETE ON calendar_events "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected calendar delete failure'); "
            "END"
            )));

        const Status eventDeleted = repository.deleteCalendarEvent(existing.id);
        QVERIFY(!eventDeleted);
        QVERIFY(eventDeleted.error().contains(
            QStringLiteral("Deleting calendar event")
            ));

        const Status seriesDeleted =
            repository.deleteCalendarEventsForRepeatSeriesFromDate(
                QStringLiteral("failure-series"),
                QDate(2026, 8, 3)
                );
        QVERIFY(!seriesDeleted);
        QVERIFY(seriesDeleted.error().contains(
            QStringLiteral("Deleting calendar repeat series events")
            ));

        const Status allDeleted = repository.deleteAllCalendarEvents();
        QVERIFY(!allDeleted);
        QVERIFY(allDeleted.error().contains(
            QStringLiteral("Deleting all calendar events")
            ));
        QCOMPARE(repository.getCalendarEvent(existing.id)->id, existing.id);
    }

    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_GUILESS_MAIN(CalendarEventRepositoryTests)

#include "calendar_event_repository_tests.moc"

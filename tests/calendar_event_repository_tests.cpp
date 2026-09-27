#include "data/repositories/calendar_event_repository.h"
#include "data/database/database_schema_manager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
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

QVariantList calendarEventRowSnapshot(
    QSqlDatabase& database,
    const int eventId
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT
            id,
            title,
            event_type,
            time_status,
            repeat_series_id,
            all_day,
            start_date,
            start_time,
            end_date,
            end_time
        FROM calendar_events
        WHERE id=?
    )");
    query.addBindValue(eventId);
    if (!query.exec() || !query.next())
    {
        return {};
    }

    QVariantList row;
    const QSqlRecord record = query.record();
    for (int column = 0; column < record.count(); ++column)
    {
        row.append(query.value(column));
    }
    return row;
}

QVariantList expectedCalendarEventRow(
    const int eventId,
    const QString& title,
    const QString& eventType,
    const QString& repeatSeriesId,
    const QString& startDate,
    const QString& startTime,
    const QString& endDate,
    const QString& endTime
    )
{
    const QVariant repeatSeriesValue = repeatSeriesId.isEmpty()
        ? QVariant()
        : QVariant(repeatSeriesId);
    return {
        eventId,
        title,
        eventType,
        QStringLiteral("Timed"),
        repeatSeriesValue,
        0,
        startDate,
        startTime,
        endDate,
        endTime
    };
}
}

class CalendarEventRepositoryTests : public QObject
{
    Q_OBJECT

private slots:
    void rangeQueryIncludesEventsThatOverlapRange();
    void intervalRangeQueryReturnsFullInclusiveIntervals();
    void rangeQuerySortsByDateTimeAndTitle();
    void upcomingQueryExcludesPastEventsAndLimitsResults();
    void nextEventQueryFindsEarliestFutureStartDate();
    void schemaCreatesEndDateIndex();
    void savesAndLoadsRepeatSeriesId();
    void repeatSeriesQueryLoadsSelectedAndFollowingOnly();
    void repeatSeriesSuffixSelectionAndPersistedUpdatesPreserveEarlierAndUnrelatedRows();
    void repeatSeriesDeleteRemovesSelectedAndFollowingOnly();
    void singleEventDeleteRemovesOnlySelectedEventAndPreservesSequence();
    void deleteAllCalendarEventsRemovesSeededRowsAndPreservesSequence();
    void singleEventCreateAndUpdateDetachOnlySelectedOccurrence();
    void writeFailuresAreReturnedAndBatchRollsBack();
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

void CalendarEventRepositoryTests::
intervalRangeQueryReturnsFullInclusiveIntervals()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_interval_range_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Historical prefix"),
                QDate(2025, 12, 20),
                QTime(0, 0),
                QDate(2026, 1, 7),
                QTime(23, 59),
                QStringLiteral("Vacation")
                )
            );
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Next year boundary"),
                QDate(2027, 12, 31),
                QTime(0, 0),
                QDate(2028, 1, 7),
                QTime(23, 59),
                QStringLiteral("Holiday")
                )
            );
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("Before window"),
                QDate(2025, 12, 1),
                QTime(9, 0),
                QDate(2025, 12, 15),
                QTime(10, 0),
                QStringLiteral("Vacation")
                )
            );
        saveCalendarEventOrFail(repository,
            makeEvent(
                QStringLiteral("After window"),
                QDate(2028, 1, 1),
                QTime(9, 0),
                QDate(2028, 1, 15),
                QTime(10, 0),
                QStringLiteral("Holiday")
                )
            );

        const auto intervals =
            repository.loadCalendarEventDateIntervalsInRange(
                QDate(2026, 1, 1),
                QDate(2027, 12, 31)
                );

        QVERIFY(intervals);
        QCOMPARE(intervals->size(), 2);
        QCOMPARE(intervals->at(0).eventType, QStringLiteral("Vacation"));
        QCOMPARE(intervals->at(0).startDate, QDate(2025, 12, 20));
        QCOMPARE(intervals->at(0).endDate, QDate(2026, 1, 7));
        QCOMPARE(intervals->at(1).eventType, QStringLiteral("Holiday"));
        QCOMPARE(intervals->at(1).startDate, QDate(2027, 12, 31));
        QCOMPARE(intervals->at(1).endDate, QDate(2028, 1, 7));
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

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(QStringLiteral(":memory:"));
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

void CalendarEventRepositoryTests::
repeatSeriesSuffixSelectionAndPersistedUpdatesPreserveEarlierAndUnrelatedRows()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_series_update_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        const Result<int> earlierCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Earlier occurrence"),
                QDate(2026, 7, 1),
                QTime(9, 0),
                QDate(2026, 7, 1),
                QTime(10, 0),
                QStringLiteral("Meeting"),
                QStringLiteral("series-1")
                )
            );
        QVERIFY(earlierCreated);
        const int earlierId = earlierCreated.value();

        const Result<int> selectedCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Selected occurrence"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0),
                QStringLiteral("Meeting"),
                QStringLiteral("series-1")
                )
            );
        QVERIFY(selectedCreated);
        const int selectedId = selectedCreated.value();

        const Result<int> followingCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Following occurrence"),
                QDate(2026, 7, 15),
                QTime(9, 0),
                QDate(2026, 7, 15),
                QTime(10, 0),
                QStringLiteral("Meeting"),
                QStringLiteral("series-1")
                )
            );
        QVERIFY(followingCreated);
        const int followingId = followingCreated.value();

        const Result<int> otherSeriesCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Other series on selected date"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0),
                QStringLiteral("Workshop"),
                QStringLiteral("series-2")
                )
            );
        QVERIFY(otherSeriesCreated);
        const int otherSeriesId = otherSeriesCreated.value();

        const Result<int> unrelatedCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Unrelated event on selected date"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0)
                )
            );
        QVERIFY(unrelatedCreated);
        const int unrelatedId = unrelatedCreated.value();

        const QList<int> allIds = {
            earlierId,
            selectedId,
            followingId,
            otherSeriesId,
            unrelatedId
        };
        QList<QVariantList> beforeSnapshots;
        for (const int eventId : allIds)
        {
            const QVariantList snapshot =
                calendarEventRowSnapshot(database, eventId);
            QVERIFY(!snapshot.isEmpty());
            beforeSnapshots.append(snapshot);
        }

        QCOMPARE(
            beforeSnapshots.at(0),
            expectedCalendarEventRow(
                earlierId,
                QStringLiteral("Earlier occurrence"),
                QStringLiteral("Meeting"),
                QStringLiteral("series-1"),
                QStringLiteral("2026-07-01"),
                QStringLiteral("09:00"),
                QStringLiteral("2026-07-01"),
                QStringLiteral("10:00")
                )
            );
        QCOMPARE(
            beforeSnapshots.at(1),
            expectedCalendarEventRow(
                selectedId,
                QStringLiteral("Selected occurrence"),
                QStringLiteral("Meeting"),
                QStringLiteral("series-1"),
                QStringLiteral("2026-07-08"),
                QStringLiteral("09:00"),
                QStringLiteral("2026-07-08"),
                QStringLiteral("10:00")
                )
            );
        QCOMPARE(
            beforeSnapshots.at(2),
            expectedCalendarEventRow(
                followingId,
                QStringLiteral("Following occurrence"),
                QStringLiteral("Meeting"),
                QStringLiteral("series-1"),
                QStringLiteral("2026-07-15"),
                QStringLiteral("09:00"),
                QStringLiteral("2026-07-15"),
                QStringLiteral("10:00")
                )
            );
        QCOMPARE(
            beforeSnapshots.at(3),
            expectedCalendarEventRow(
                otherSeriesId,
                QStringLiteral("Other series on selected date"),
                QStringLiteral("Workshop"),
                QStringLiteral("series-2"),
                QStringLiteral("2026-07-08"),
                QStringLiteral("09:00"),
                QStringLiteral("2026-07-08"),
                QStringLiteral("10:00")
                )
            );
        const QVariantList expectedUnrelatedRow =
            expectedCalendarEventRow(
                unrelatedId,
                QStringLiteral("Unrelated event on selected date"),
                QStringLiteral("Other"),
                QString(),
                QStringLiteral("2026-07-08"),
                QStringLiteral("09:00"),
                QStringLiteral("2026-07-08"),
                QStringLiteral("10:00")
                );
        QCOMPARE(
            beforeSnapshots.at(4).mid(0, 4)
                + beforeSnapshots.at(4).mid(5),
            expectedUnrelatedRow.mid(0, 4) + expectedUnrelatedRow.mid(5)
            );
        QVERIFY(beforeSnapshots.at(4).at(4).isNull());

        QSqlQuery countQuery(database);
        QVERIFY(countQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM calendar_events"
            )));
        QVERIFY(countQuery.next());
        const qint64 rowCountBefore = countQuery.value(0).toLongLong();
        QCOMPARE(rowCountBefore, 5);
        QVERIFY(!countQuery.next());

        QSqlQuery sequenceQuery(database);
        QVERIFY(sequenceQuery.exec(QStringLiteral(
            "SELECT seq FROM sqlite_sequence WHERE name='calendar_events'"
            )));
        QVERIFY(sequenceQuery.next());
        const qint64 sequenceBefore = sequenceQuery.value(0).toLongLong();
        QCOMPARE(sequenceBefore, 5);
        QVERIFY(!sequenceQuery.next());

        const Result<QList<CalendarEvent>> selectedAndFollowing =
            repository.loadCalendarEventsForRepeatSeriesFromDate(
                QStringLiteral(" series-1 "),
                QDate(2026, 7, 8)
                );
        QVERIFY(selectedAndFollowing);
        QCOMPARE(selectedAndFollowing->size(), 2);

        QList<int> selectedIdsBeforeUpdate;
        for (const CalendarEvent& event : *selectedAndFollowing)
        {
            selectedIdsBeforeUpdate.append(event.id);
        }
        QCOMPARE(
            selectedIdsBeforeUpdate,
            QList<int>({selectedId, followingId})
            );

        QList<CalendarEvent> updatedEvents = *selectedAndFollowing;
        for (CalendarEvent& event : updatedEvents)
        {
            event.title += QStringLiteral(" - edited");
            event.startDate = event.startDate.addDays(1);
            event.startTime = event.startTime.addSecs(30 * 60);
            event.endDate = event.endDate.addDays(1);
            event.endTime = event.endTime.addSecs(30 * 60);
        }
        const Result<QList<int>> saved =
            repository.saveCalendarEvents(updatedEvents);
        QVERIFY(saved);
        QCOMPARE(saved.value(), selectedIdsBeforeUpdate);

        QList<QVariantList> afterSnapshots;
        for (const int eventId : allIds)
        {
            const QVariantList snapshot =
                calendarEventRowSnapshot(database, eventId);
            QVERIFY(!snapshot.isEmpty());
            afterSnapshots.append(snapshot);
        }

        QCOMPARE(afterSnapshots.at(0), beforeSnapshots.at(0));
        QCOMPARE(afterSnapshots.at(3), beforeSnapshots.at(3));
        QCOMPARE(afterSnapshots.at(4), beforeSnapshots.at(4));
        QCOMPARE(
            afterSnapshots.at(1),
            expectedCalendarEventRow(
                selectedId,
                QStringLiteral("Selected occurrence - edited"),
                QStringLiteral("Meeting"),
                QStringLiteral("series-1"),
                QStringLiteral("2026-07-09"),
                QStringLiteral("09:30"),
                QStringLiteral("2026-07-09"),
                QStringLiteral("10:30")
                )
            );
        QCOMPARE(
            afterSnapshots.at(2),
            expectedCalendarEventRow(
                followingId,
                QStringLiteral("Following occurrence - edited"),
                QStringLiteral("Meeting"),
                QStringLiteral("series-1"),
                QStringLiteral("2026-07-16"),
                QStringLiteral("09:30"),
                QStringLiteral("2026-07-16"),
                QStringLiteral("10:30")
                )
            );

        const Result<QList<CalendarEvent>> updatedSuffix =
            repository.loadCalendarEventsForRepeatSeriesFromDate(
                QStringLiteral("series-1"),
                QDate(2026, 7, 8)
                );
        QVERIFY(updatedSuffix);
        QList<int> selectedIdsAfterUpdate;
        for (const CalendarEvent& event : *updatedSuffix)
        {
            selectedIdsAfterUpdate.append(event.id);
        }
        QCOMPARE(
            selectedIdsAfterUpdate,
            QList<int>({selectedId, followingId})
            );

        QVERIFY(countQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM calendar_events"
            )));
        QVERIFY(countQuery.next());
        const qint64 rowCountAfter = countQuery.value(0).toLongLong();
        QCOMPARE(rowCountAfter, rowCountBefore);
        QVERIFY(!countQuery.next());

        QVERIFY(sequenceQuery.exec(QStringLiteral(
            "SELECT seq FROM sqlite_sequence WHERE name='calendar_events'"
            )));
        QVERIFY(sequenceQuery.next());
        const qint64 sequenceAfter = sequenceQuery.value(0).toLongLong();
        QCOMPARE(sequenceAfter, sequenceBefore);
        QVERIFY(!sequenceQuery.next());
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

void CalendarEventRepositoryTests::
singleEventDeleteRemovesOnlySelectedEventAndPreservesSequence()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_single_delete_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        const Result<int> targetCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Target Event"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-single-delete")
                )
            );
        QVERIFY(targetCreated);
        const int targetId = targetCreated.value();

        const Result<int> siblingCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Sibling Event"),
                QDate(2026, 7, 15),
                QTime(9, 0),
                QDate(2026, 7, 15),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-single-delete")
                )
            );
        QVERIFY(siblingCreated);
        const int siblingId = siblingCreated.value();

        const Result<int> unrelatedCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Unrelated Event"),
                QDate(2026, 7, 8),
                QTime(11, 0),
                QDate(2026, 7, 8),
                QTime(12, 0)
                )
            );
        QVERIFY(unrelatedCreated);
        const int unrelatedId = unrelatedCreated.value();

        const QVariantList siblingBefore =
            calendarEventRowSnapshot(database, siblingId);
        const QVariantList unrelatedBefore =
            calendarEventRowSnapshot(database, unrelatedId);
        QVERIFY(!siblingBefore.isEmpty());
        QVERIFY(!unrelatedBefore.isEmpty());
        QVERIFY(!calendarEventRowSnapshot(database, targetId).isEmpty());

        QSqlQuery sequenceQuery(database);
        QVERIFY(sequenceQuery.exec(QStringLiteral(
            "SELECT seq FROM sqlite_sequence WHERE name='calendar_events'"
            )));
        QVERIFY(sequenceQuery.next());
        const int sequenceBeforeDelete = sequenceQuery.value(0).toInt();
        QVERIFY(sequenceBeforeDelete >= unrelatedId);
        QVERIFY(!sequenceQuery.next());

        const Status deleted = repository.deleteCalendarEvent(targetId);
        QVERIFY(deleted);
        QVERIFY(!repository.getCalendarEvent(targetId).has_value());
        QVERIFY(calendarEventRowSnapshot(database, targetId).isEmpty());
        QCOMPARE(
            calendarEventRowSnapshot(database, siblingId),
            siblingBefore
            );
        QCOMPARE(
            calendarEventRowSnapshot(database, unrelatedId),
            unrelatedBefore
            );

        QSqlQuery countQuery(database);
        QVERIFY(countQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM calendar_events"
            )));
        QVERIFY(countQuery.next());
        QCOMPARE(countQuery.value(0).toInt(), 2);
        QVERIFY(!countQuery.next());

        QVERIFY(sequenceQuery.exec(QStringLiteral(
            "SELECT seq FROM sqlite_sequence WHERE name='calendar_events'"
            )));
        QVERIFY(sequenceQuery.next());
        QCOMPARE(sequenceQuery.value(0).toInt(), sequenceBeforeDelete);
        QVERIFY(!sequenceQuery.next());
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::
deleteAllCalendarEventsRemovesSeededRowsAndPreservesSequence()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_delete_all_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        const Result<int> firstCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("First Event"),
                QDate(2026, 7, 8),
                QTime(9, 0),
                QDate(2026, 7, 8),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-delete-all")
                )
            );
        QVERIFY(firstCreated);
        const int firstId = firstCreated.value();

        const Result<int> secondCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Second Event"),
                QDate(2026, 7, 15),
                QTime(9, 0),
                QDate(2026, 7, 15),
                QTime(10, 0),
                QStringLiteral("Other"),
                QStringLiteral("series-delete-all")
                )
            );
        QVERIFY(secondCreated);
        const int secondId = secondCreated.value();

        const Result<int> thirdCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Third Event"),
                QDate(2026, 7, 22),
                QTime(11, 0),
                QDate(2026, 7, 22),
                QTime(12, 0)
                )
            );
        QVERIFY(thirdCreated);
        const int thirdId = thirdCreated.value();

        const auto eventsBefore = repository.loadCalendarEventsInRange(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            );
        QVERIFY(eventsBefore.has_value());
        QCOMPARE(eventsBefore->size(), 3);
        QVERIFY(repository.getCalendarEvent(firstId).has_value());
        QVERIFY(repository.getCalendarEvent(secondId).has_value());
        QVERIFY(repository.getCalendarEvent(thirdId).has_value());
        QVERIFY(!calendarEventRowSnapshot(database, firstId).isEmpty());
        QVERIFY(!calendarEventRowSnapshot(database, secondId).isEmpty());
        QVERIFY(!calendarEventRowSnapshot(database, thirdId).isEmpty());

        QSqlQuery countQuery(database);
        QVERIFY(countQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM calendar_events"
            )));
        QVERIFY(countQuery.next());
        QCOMPARE(countQuery.value(0).toInt(), 3);
        QVERIFY(!countQuery.next());

        QSqlQuery sequenceQuery(database);
        QVERIFY(sequenceQuery.exec(QStringLiteral(
            "SELECT seq FROM sqlite_sequence WHERE name='calendar_events'"
            )));
        QVERIFY(sequenceQuery.next());
        const qint64 sequenceBeforeDelete =
            sequenceQuery.value(0).toLongLong();
        QVERIFY(sequenceBeforeDelete >= thirdId);
        QVERIFY(!sequenceQuery.next());

        const Status deleted = repository.deleteAllCalendarEvents();
        QVERIFY(deleted);

        QVERIFY(!repository.getCalendarEvent(firstId).has_value());
        QVERIFY(!repository.getCalendarEvent(secondId).has_value());
        QVERIFY(!repository.getCalendarEvent(thirdId).has_value());
        QVERIFY(calendarEventRowSnapshot(database, firstId).isEmpty());
        QVERIFY(calendarEventRowSnapshot(database, secondId).isEmpty());
        QVERIFY(calendarEventRowSnapshot(database, thirdId).isEmpty());

        const auto eventsAfter = repository.loadCalendarEventsInRange(
            QDate(2026, 7, 1),
            QDate(2026, 7, 31)
            );
        QVERIFY(eventsAfter.has_value());
        QVERIFY(eventsAfter->isEmpty());

        QVERIFY(countQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM calendar_events"
            )));
        QVERIFY(countQuery.next());
        QCOMPARE(countQuery.value(0).toInt(), 0);
        QVERIFY(!countQuery.next());

        QVERIFY(sequenceQuery.exec(QStringLiteral(
            "SELECT seq FROM sqlite_sequence WHERE name='calendar_events'"
            )));
        QVERIFY(sequenceQuery.next());
        QCOMPARE(
            sequenceQuery.value(0).toLongLong(),
            sequenceBeforeDelete
            );
        QVERIFY(!sequenceQuery.next());
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::
singleEventCreateAndUpdateDetachOnlySelectedOccurrence()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_single_save_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));

        QVERIFY(database.open());
        createCalendarEventsTable(database);

        CalendarEventRepository repository(database);
        const Result<int> selectedCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Original occurrence"),
                QDate(2026, 9, 10),
                QTime(9, 0),
                QDate(2026, 9, 10),
                QTime(10, 0),
                QStringLiteral("Meeting"),
                QStringLiteral("shared-series")
                )
            );
        QVERIFY(selectedCreated);
        const int selectedId = selectedCreated.value();
        QVERIFY(selectedId > 0);

        const Result<int> siblingCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Sibling occurrence"),
                QDate(2026, 9, 17),
                QTime(9, 0),
                QDate(2026, 9, 17),
                QTime(10, 0),
                QStringLiteral("Meeting"),
                QStringLiteral("shared-series")
                )
            );
        QVERIFY(siblingCreated);
        const int siblingId = siblingCreated.value();

        const Result<int> unrelatedCreated = repository.saveCalendarEvent(
            makeEvent(
                QStringLiteral("Unrelated event"),
                QDate(2026, 9, 18),
                QTime(11, 0),
                QDate(2026, 9, 18),
                QTime(12, 0),
                QStringLiteral("Workshop"),
                QStringLiteral("other-series")
                )
            );
        QVERIFY(unrelatedCreated);
        const int unrelatedId = unrelatedCreated.value();

        const QVariantList siblingBefore =
            calendarEventRowSnapshot(database, siblingId);
        const QVariantList unrelatedBefore =
            calendarEventRowSnapshot(database, unrelatedId);
        QCOMPARE(siblingBefore.size(), 10);
        QCOMPARE(unrelatedBefore.size(), 10);

        CalendarEvent createdEvent = makeEvent(
            QStringLiteral("Created standalone event"),
            QDate(2026, 9, 19),
            QTime(13, 15),
            QDate(2026, 9, 19),
            QTime(14, 0),
            QStringLiteral("  Vacation  ")
            );
        const Result<int> created = repository.saveCalendarEvent(createdEvent);
        QVERIFY(created);
        const int createdId = created.value();
        QVERIFY(createdId > 0);
        QVERIFY(createdId != selectedId);
        QVERIFY(createdId != siblingId);
        QVERIFY(createdId != unrelatedId);

        const QVariantList createdRow =
            calendarEventRowSnapshot(database, createdId);
        QCOMPARE(createdRow.size(), 10);
        QCOMPARE(createdRow.at(0).toInt(), createdId);
        QCOMPARE(createdRow.at(1).toString(),
                 QStringLiteral("Created standalone event"));
        QCOMPARE(createdRow.at(2).toString(), QStringLiteral("Vacation"));
        QCOMPARE(createdRow.at(3).toString(), QStringLiteral("Timed"));
        QVERIFY(createdRow.at(4).isNull());
        QCOMPARE(createdRow.at(5).toInt(), 0);
        QCOMPARE(createdRow.at(6).toString(), QStringLiteral("2026-09-19"));
        QCOMPARE(createdRow.at(7).toString(), QStringLiteral("13:15"));
        QCOMPARE(createdRow.at(8).toString(), QStringLiteral("2026-09-19"));
        QCOMPARE(createdRow.at(9).toString(), QStringLiteral("14:00"));

        CalendarEvent detachedUpdate = makeEvent(
            QStringLiteral("Updated detached occurrence"),
            QDate(2026, 9, 11),
            QTime(15, 30),
            QDate(2026, 9, 11),
            QTime(16, 15),
            QStringLiteral("  Holiday  ")
            );
        detachedUpdate.id = selectedId;
        const Result<int> updated =
            repository.saveCalendarEvent(detachedUpdate);
        QVERIFY(updated);
        QCOMPARE(updated.value(), selectedId);

        const QVariantList selectedAfter =
            calendarEventRowSnapshot(database, selectedId);
        QCOMPARE(selectedAfter.size(), 10);
        QCOMPARE(selectedAfter.at(0).toInt(), selectedId);
        QCOMPARE(selectedAfter.at(1).toString(),
                 QStringLiteral("Updated detached occurrence"));
        QCOMPARE(selectedAfter.at(2).toString(), QStringLiteral("Holiday"));
        QCOMPARE(selectedAfter.at(3).toString(), QStringLiteral("Timed"));
        QVERIFY(selectedAfter.at(4).isNull());
        QCOMPARE(selectedAfter.at(5).toInt(), 0);
        QCOMPARE(selectedAfter.at(6).toString(), QStringLiteral("2026-09-11"));
        QCOMPARE(selectedAfter.at(7).toString(), QStringLiteral("15:30"));
        QCOMPARE(selectedAfter.at(8).toString(), QStringLiteral("2026-09-11"));
        QCOMPARE(selectedAfter.at(9).toString(), QStringLiteral("16:15"));
        QCOMPARE(calendarEventRowSnapshot(database, siblingId), siblingBefore);
        QCOMPARE(calendarEventRowSnapshot(database, unrelatedId), unrelatedBefore);
        QCOMPARE(calendarEventRowSnapshot(database, createdId), createdRow);

        QSqlQuery sequenceQuery(database);
        QVERIFY(sequenceQuery.exec(QStringLiteral(
            "SELECT seq FROM sqlite_sequence WHERE name='calendar_events'"
            )));
        QVERIFY(sequenceQuery.next());
        const int sequenceAfterCreate = sequenceQuery.value(0).toInt();
        QVERIFY(sequenceAfterCreate >= createdId);
        QVERIFY(!sequenceQuery.next());

        const Result<int> updatedAgain =
            repository.saveCalendarEvent(detachedUpdate);
        QVERIFY(updatedAgain);
        QCOMPARE(updatedAgain.value(), selectedId);
        QVERIFY(sequenceQuery.exec(QStringLiteral(
            "SELECT seq FROM sqlite_sequence WHERE name='calendar_events'"
            )));
        QVERIFY(sequenceQuery.next());
        QCOMPARE(sequenceQuery.value(0).toInt(), sequenceAfterCreate);
        QVERIFY(!sequenceQuery.next());
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CalendarEventRepositoryTests::writeFailuresAreReturnedAndBatchRollsBack()
{
    const QString connectionName =
        QStringLiteral("calendar_event_repository_failure_tests");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());
        createCalendarEventsTable(database);

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TRIGGER reject_calendar_insert "
            "BEFORE INSERT ON calendar_events "
            "WHEN NEW.title = 'Reject' "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected calendar insert failure'); "
            "END"
            )));

        CalendarEventRepository repository(database);
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

QTEST_MAIN(CalendarEventRepositoryTests)

#include "calendar_event_repository_tests.moc"

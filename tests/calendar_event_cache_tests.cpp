#include "data/repositories/calendar_event_repository.h"
#include "features/calendar/calendar_event_campus_filter.h"
#include "features/calendar/calendar_event_projection_query.h"
#include "features/calendar/ui/calendar_event_cache.h"
#include "features/calendar/ui/calendar_event_model.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QSemaphore>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <atomic>
#include <memory>
#include <optional>
#include <string>

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
    const Result<qint64> saved = repository.saveCalendarEvent(event);
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

QString projectionTimeStatusText(
    const ClassMngr::Next::Domain::CalendarEventTimeStatus value
    )
{
    const auto name =
        ClassMngr::Next::Domain::calendarEventTimeStatusName(value);
    if (!name)
    {
        return {};
    }

    return QString::fromLatin1(
        name->data(),
        static_cast<qsizetype>(name->size())
        );
}

QString repeatedHangulTitle(const int count)
{
    return QString(count, QChar(0xAC00));
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

struct InjectedQueryState final
{
    int createCalls = 0;
    int rangeCalls = 0;
    int nextEventCalls = 0;
    std::optional<
        ClassMngr::Next::Application::CalendarEventProjection
        > rangeProjection;
    std::optional<
        ClassMngr::Next::Application::CalendarEventRangeRequest
        > lastRangeRequest;
    std::optional<
        ClassMngr::Next::Application::CalendarEventNextEventRequest
        > lastNextEventRequest;
};

ClassMngr::Next::Application::CalendarEventProjection injectedProjection(
    const std::string& title = "Injected event"
    )
{
    ClassMngr::Next::Application::CalendarEventProjectionInput input;
    input.events.push_back({
        *ClassMngr::Next::Domain::CalendarEventId::fromString("42"),
        std::nullopt,
        std::nullopt,
        title,
        "2026-07-10",
        "2026-07-11",
        std::string("09:00"),
        std::string("10:00"),
        {},
        {},
        0,
        false,
        "Meeting",
        ClassMngr::Next::Domain::CalendarEventTimeStatus::Timed,
        *ClassMngr::Next::Domain::CalendarEventSeriesId::fromString(
            "series-injected"
            )
    });
    const auto projection =
        ClassMngr::Next::Application::CalendarEventProjection::create(
            std::move(input)
            );
    Q_ASSERT(projection);
    return projection.value();
}

class InjectedQueryPort final
    : public ClassMngr::Next::Application::CalendarEventQueryPort
{
public:
    explicit InjectedQueryPort(
        std::shared_ptr<InjectedQueryState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] ClassMngr::Next::Application::CalendarEventProjectionResult
    loadRange(
        const ClassMngr::Next::Application::CalendarEventRangeRequest& request
        ) override
    {
        ++m_state->rangeCalls;
        m_state->lastRangeRequest = request;
        if (m_state->rangeProjection)
        {
            return ClassMngr::Next::Application::
                CalendarEventProjectionResult::success(
                    *m_state->rangeProjection
                    );
        }
        return ClassMngr::Next::Application::CalendarEventProjectionResult::success(
            injectedProjection()
            );
    }

    [[nodiscard]] ClassMngr::Next::Application::CalendarEventDateResult
    findNextEventDate(
        const ClassMngr::Next::Application::CalendarEventNextEventRequest& request
        ) override
    {
        ++m_state->nextEventCalls;
        m_state->lastNextEventRequest = request;
        return ClassMngr::Next::Application::CalendarEventDateResult::success(
            ClassMngr::Next::Application::CalendarEventDate("2026-07-10")
            );
    }

private:
    std::shared_ptr<InjectedQueryState> m_state;
};

class InjectedQueryFactory final
    : public ClassMngr::Next::Application::CalendarEventQueryPortFactory
{
public:
    explicit InjectedQueryFactory(
        std::shared_ptr<InjectedQueryState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] std::unique_ptr<
        ClassMngr::Next::Application::CalendarEventQueryPort
        > create() const override
    {
        ++m_state->createCalls;
        return std::make_unique<InjectedQueryPort>(m_state);
    }

private:
    std::shared_ptr<InjectedQueryState> m_state;
};

struct BlockedQueryState final
{
    QSemaphore entered;
    QSemaphore resume;
    std::atomic<int> calls = 0;
    std::atomic_bool blockQueries = true;
    std::atomic_bool failAllRanges = false;
    std::atomic_bool failFirstRange = false;
    std::atomic_bool failFirstNextEvent = false;
};

class BlockedQueryPort final
    : public ClassMngr::Next::Application::CalendarEventQueryPort
{
public:
    explicit BlockedQueryPort(std::shared_ptr<BlockedQueryState> state)
        : m_state(std::move(state))
    {
    }

    ClassMngr::Next::Application::CalendarEventProjectionResult loadRange(
        const ClassMngr::Next::Application::CalendarEventRangeRequest&
        ) override
    {
        const int call = enter();
        if (
            m_state->failAllRanges.load()
            || (call == 1 && m_state->failFirstRange.load())
            )
        {
            return ClassMngr::Next::Application::
                CalendarEventProjectionResult::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                    .message = "injected calendar range failure",
                    .recoverable = true
                });
        }

        return ClassMngr::Next::Application::CalendarEventProjectionResult::success(
            injectedProjection(call == 1 ? "Stale generation" : "Fresh generation"));
    }

    ClassMngr::Next::Application::CalendarEventDateResult findNextEventDate(
        const ClassMngr::Next::Application::CalendarEventNextEventRequest&
        ) override
    {
        const int call = enter();
        if (call == 1 && m_state->failFirstNextEvent.load())
        {
            return ClassMngr::Next::Application::CalendarEventDateResult::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                .message = "injected calendar next event failure",
                .recoverable = true
            });
        }

        return ClassMngr::Next::Application::CalendarEventDateResult::success(
            ClassMngr::Next::Application::CalendarEventDate(
                call == 1 ? "2026-07-10" : "2026-08-10"));
    }

private:
    int enter()
    {
        const int call = ++m_state->calls;
        if (m_state->blockQueries.load())
        {
            m_state->entered.release();
            m_state->resume.acquire();
        }
        return call;
    }
    std::shared_ptr<BlockedQueryState> m_state;
};

class BlockedQueryFactory final
    : public ClassMngr::Next::Application::CalendarEventQueryPortFactory
{
public:
    explicit BlockedQueryFactory(std::shared_ptr<BlockedQueryState> state)
        : m_state(std::move(state))
    {
    }
    std::unique_ptr<ClassMngr::Next::Application::CalendarEventQueryPort>
    create() const override
    {
        return std::make_unique<BlockedQueryPort>(m_state);
    }
private:
    std::shared_ptr<BlockedQueryState> m_state;
};

struct ResumeQueriesOnExit final
{
    std::shared_ptr<BlockedQueryState> state;
    ~ResumeQueriesOnExit() { state->resume.release(8); }
};
}

class CalendarEventCacheTests : public QObject
{
    Q_OBJECT

private slots:
    void wideIdsSurviveCacheAndModel_data();
    void wideIdsSurviveCacheAndModel();
    void rangeLoadPopulatesModelWithoutUiThreadDatabaseAccess();
    void injectedQueryFactoryFeedsLegacyCacheBoundary();
    void injectedInvalidTimesAreFilteredByCacheAndModel();
    void projectionMappingPreservesRichCalendarFieldsAndCleansConnections();
    void projectionQueryAndCacheUseUtf16TitleBounds();
    void projectionQueryBoundsRetainedRowsAndPreservesConversionErrors();
    void typedDateProjectionPreservesModelParityAndFiltering();
    void typedRangeProjectionPreservesOrderingMetadataAndLegacyParity();
    void nextEventLookupUsesProjectionQueryAndDeduplicates();
    void nextEventLookupFailureExposesDiagnosticAndRetryClearsIt();
    void nextEventLookupFailureDiagnosticClearsOnInvalidation();
    void rangeFailureExposesDiagnosticAndRetryClearsIt();
    void rangeFailureDiagnosticFollowsRetainedRanges();
    void staleGenerationRangeFailureDoesNotLeak();
    void invalidationDiscardsCompletedWorkerResult();
    void invalidatedActiveRangeDoesNotSuppressSameRangeReissue();
    void invalidatedActiveNextEventLookupDoesNotSuppressReissue();
    void multiDayEventsUseOneCanonicalRecordAndRangeDeduplicates();
    void retainedRangesEvictEventsAndRejectEvictedWorkerResults();
};

void CalendarEventCacheTests::wideIdsSurviveCacheAndModel_data()
{
    QTest::addColumn<qint64>("wideId");
    QTest::newRow("above-int-max") << qint64(2147483648LL);
    QTest::newRow("above-javascript-exact-range") << qint64(9007199254740993LL);
    QTest::newRow("signed-64-max") << qint64(9223372036854775807LL);
}

void CalendarEventCacheTests::wideIdsSurviveCacheAndModel()
{
    QFETCH(qint64, wideId);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("wide-cache.db");
    const QString connection = QUuid::createUuid().toString();
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", connection);
        database.setDatabaseName(path);
        QVERIFY(database.open());
        createCalendarEventsTable(database);
        QSqlQuery query(database);
        query.prepare("INSERT INTO calendar_events "
                      "(id,title,start_date,end_date,start_time,end_time) "
                      "VALUES (?,'Wide cache event','2026-07-10','2026-07-11','09:00','10:00')");
        query.addBindValue(wideId);
        QVERIFY(query.exec());
        QVERIFY(query.exec("INSERT INTO calendar_events "
                           "(id,title,start_date,end_date,start_time,end_time) "
                           "VALUES (1,'Low cache event','2026-07-10','2026-07-10','09:00','10:00')"));
    }
    QSqlDatabase::removeDatabase(connection);
    CalendarEventCache cache;
    CalendarEventModel model(&cache);
    cache.setDatabasePath(path);
    cache.requestRange(QDate(2026,7,1), QDate(2026,7,31));
    QTRY_VERIFY_WITH_TIMEOUT(cache.isRangeLoaded(QDate(2026,7,1), QDate(2026,7,31)), 5000);
    QCOMPARE(cache.eventCount(), 2);
    QCOMPARE(cache.eventsInRange(QDate(2026,7,10), QDate(2026,7,11)).size(), 2);
    const auto day = cache.eventsForDate(QDate(2026,7,11));
    QCOMPARE(day.size(), 1);
    QCOMPARE(day.front().id, wideId);
    const auto projection = cache.eventProjectionForDate(QDate(2026,7,11));
    QCOMPARE(projection.events().front().id.value(), std::to_string(wideId));
    const auto values = model.eventsForDate(2026,7,11);
    QCOMPARE(values.size(), 1);
    const auto id = values.front().toMap().value("id");
    QCOMPARE(id.metaType(), QMetaType::fromType<QString>());
    QCOMPARE(id.toString(), QString::number(wideId));
    cache.setRetainedRanges({{QDate(2026,7,11), QDate(2026,7,11)}});
    QCOMPARE(cache.eventCount(), 1);
    QCOMPARE(cache.eventsForDate(QDate(2026,7,11)).front().id, wideId);
}

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

void CalendarEventCacheTests::injectedQueryFactoryFeedsLegacyCacheBoundary()
{
    const auto state = std::make_shared<InjectedQueryState>();
    const auto factory = std::make_shared<InjectedQueryFactory>(state);
    CalendarEventCache cache(factory);
    cache.setDatabasePath(QStringLiteral("worker-calendar.db"));

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

    QCOMPARE(state->createCalls, 1);
    QCOMPARE(state->rangeCalls, 1);
    QCOMPARE(
        state->lastRangeRequest->databasePath,
        std::string("worker-calendar.db")
        );
    QCOMPARE(
        state->lastRangeRequest->startDate.value(),
        std::string("2026-07-01")
        );
    QCOMPARE(
        state->lastRangeRequest->endDate.value(),
        std::string("2026-07-31")
        );

    const auto events = cache.eventsForDate(QDate(2026, 7, 10));
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().id, 42);
    QCOMPARE(events.first().title, QStringLiteral("Injected event"));
    QCOMPARE(
        events.first().repeatSeriesId,
        QStringLiteral("series-injected")
        );
    QCOMPARE(events.first().startTime, QTime(9, 0));

    const auto projection = cache.eventProjectionForDate(
        QDate(2026, 7, 10)
        );
    QCOMPARE(projection.eventCount(), std::size_t(1));
    QCOMPARE(projection.events().front().id.value(), std::string("42"));
    QCOMPARE(
        projection.events().front().repeatSeriesId,
        std::optional<ClassMngr::Next::Domain::CalendarEventSeriesId>(
            *ClassMngr::Next::Domain::CalendarEventSeriesId::fromString(
                "series-injected"
                )
            )
        );

    CalendarEventModel model(&cache);
    const QVariantList modelEvents = model.eventsForDate(2026, 7, 10);
    QCOMPARE(modelEvents.size(), 1);
    QCOMPARE(
        modelEvents.first().toMap().value(QStringLiteral("id")).toInt(),
        42
        );
    QCOMPARE(
        modelEvents.first().toMap().value(QStringLiteral("title")).toString(),
        QStringLiteral("Injected event")
        );

    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            projection.events().front(),
            {QStringLiteral("BDG")},
            {QStringLiteral("BDG"), QStringLiteral("SNU")},
            false
            )
        );
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            QStringLiteral("Injected event"),
            {QStringLiteral("BDG")},
            {QStringLiteral("BDG"), QStringLiteral("SNU")},
            false
            )
        );

    QSignalSpy nextEventSpy(
        &cache,
        &CalendarEventCache::nextEventMonthFound
        );
    cache.requestNextEventMonth(QDate(2026, 7, 1));
    QTRY_COMPARE_WITH_TIMEOUT(nextEventSpy.count(), 1, 5000);
    QCOMPARE(nextEventSpy.at(0).at(0).toDate(), QDate(2026, 7, 10));
    QCOMPARE(state->createCalls, 2);
    QCOMPARE(state->nextEventCalls, 1);
}

void CalendarEventCacheTests::injectedInvalidTimesAreFilteredByCacheAndModel()
{
    const auto state = std::make_shared<InjectedQueryState>();

    const auto timedSummary = [](
        const std::string& id,
        const std::string& title,
        std::string startTime,
        std::string endTime,
        const int order
        )
    {
        return ClassMngr::Next::Application::CalendarEventSummary{
            *ClassMngr::Next::Domain::CalendarEventId::fromString(id),
            std::nullopt,
            std::nullopt,
            title,
            "2026-07-10",
            "2026-07-10",
            std::move(startTime),
            std::move(endTime),
            {},
            {},
            order,
            false,
            "Meeting",
            ClassMngr::Next::Domain::CalendarEventTimeStatus::Timed,
            std::nullopt
        };
    };

    ClassMngr::Next::Application::CalendarEventProjectionInput input;
    input.events.push_back(
        timedSummary("42", "Canonical event", "09:00", "10:00", 0)
        );
    input.events.push_back(
        timedSummary("43", "Malformed event", "not-a-time", "10:00", 1)
        );
    input.events.push_back(
        timedSummary("44", "Whitespace event", " 09:00 ", "10:00", 2)
        );
    input.events.push_back(
        timedSummary("45", "Malformed end event", "09:00", "not-a-time", 3)
        );
    input.events.push_back(
        timedSummary("46", "Whitespace end event", "09:00", " 10:00 ", 4)
        );
    const auto projection =
        ClassMngr::Next::Application::CalendarEventProjection::create(
            std::move(input)
            );
    QVERIFY(projection);
    state->rangeProjection = projection.value();

    CalendarEventCache cache(
        std::make_shared<InjectedQueryFactory>(state)
        );
    cache.setDatabasePath(QStringLiteral("injected-invalid-time-calendar.db"));

    const QDate eventDate(2026, 7, 10);
    cache.requestRange(eventDate, eventDate);
    QTRY_VERIFY_WITH_TIMEOUT(
        cache.isRangeLoaded(eventDate, eventDate),
        5000
        );

    const auto typedProjection = cache.eventProjectionForDate(eventDate);
    QCOMPARE(typedProjection.eventCount(), std::size_t(5));

    const QList<CalendarEvent> legacyEvents =
        cache.eventsForDate(eventDate);
    QCOMPARE(legacyEvents.size(), 1);
    QCOMPARE(legacyEvents.first().title, QStringLiteral("Canonical event"));

    CalendarEventModel model(&cache);
    const QVariantList modelEvents = model.eventsForDate(2026, 7, 10);
    QCOMPARE(modelEvents.size(), 1);
    const QVariantMap modelEvent = modelEvents.first().toMap();
    QCOMPARE(
        modelEvent.value(QStringLiteral("title")).toString(),
        QStringLiteral("Canonical event")
        );
    QCOMPARE(
        modelEvent.value(QStringLiteral("start")).toDateTime(),
        QDateTime(legacyEvents.first().startDate, legacyEvents.first().startTime)
        );
    QCOMPARE(
        modelEvent.value(QStringLiteral("end")).toDateTime(),
        QDateTime(legacyEvents.first().endDate, legacyEvents.first().endTime)
        );
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
    QCOMPARE(
        repeat->timeStatus,
        ClassMngr::Next::Domain::CalendarEventTimeStatus::Unknown
        );
    QCOMPARE(
        repeat->repeatSeriesId,
        std::optional<ClassMngr::Next::Domain::CalendarEventSeriesId>(
            *ClassMngr::Next::Domain::CalendarEventSeriesId::fromString(
                "series-1"
                )
            )
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

void CalendarEventCacheTests::projectionQueryAndCacheUseUtf16TitleBounds()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    const QString acceptedTitle = repeatedHangulTitle(255);
    QCOMPARE(acceptedTitle.toUtf8().size(), 765);
    const QString connectionName =
        QStringLiteral("calendar-event-cache-title-boundary-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            );
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        insertRawEvent(
            database,
            acceptedTitle,
            QStringLiteral("Other"),
            QStringLiteral("Unknown"),
            QString(),
            false,
            QDate(2026, 7, 15),
            QString(),
            QDate(2026, 7, 15),
            QString()
            );
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const auto queried = CalendarEventProjectionQuery::loadRange(
        databasePath,
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );
    QVERIFY(queried);
    QCOMPARE(queried.value().eventCount(), std::size_t(2));
    const std::string acceptedTitleUtf8 = acceptedTitle.toUtf8().toStdString();
    bool queryProjectedAcceptedTitle = false;
    for (const auto& event : queried.value().events())
    {
        if (event.title == acceptedTitleUtf8)
        {
            queryProjectedAcceptedTitle = true;
            break;
        }
    }
    QVERIFY(queryProjectedAcceptedTitle);

    CalendarEventCache cache;
    cache.setDatabasePath(databasePath);
    const QDate rangeStart(2026, 7, 1);
    const QDate rangeEnd(2026, 7, 31);
    cache.requestRange(rangeStart, rangeEnd);
    QTRY_VERIFY_WITH_TIMEOUT(
        cache.isRangeLoaded(rangeStart, rangeEnd),
        5000
        );
    const auto cached = cache.eventProjectionInRange(rangeStart, rangeEnd);
    bool cacheProjectedAcceptedTitle = false;
    for (const auto& event : cached.events())
    {
        if (event.title == acceptedTitleUtf8)
        {
            cacheProjectedAcceptedTitle = true;
            break;
        }
    }
    QVERIFY(cacheProjectedAcceptedTitle);

    const QString rejectedTitle = repeatedHangulTitle(256);
    QCOMPARE(rejectedTitle.toUtf8().size(), 768);
    const QString rejectedConnectionName =
        QStringLiteral("calendar-event-cache-overlong-title-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            );
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            rejectedConnectionName
            );
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        insertRawEvent(
            database,
            rejectedTitle,
            QStringLiteral("Other"),
            QStringLiteral("Unknown"),
            QString(),
            false,
            QDate(2026, 7, 16),
            QString(),
            QDate(2026, 7, 16),
            QString()
            );
        database.close();
    }
    QSqlDatabase::removeDatabase(rejectedConnectionName);

    const auto rejectedQuery = CalendarEventProjectionQuery::loadRange(
        databasePath,
        rangeStart,
        rangeEnd
        );
    QVERIFY(!rejectedQuery);
    QCOMPARE(
        rejectedQuery.error().code,
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );

    cache.invalidate();
    cache.requestRange(rangeStart, rangeEnd);
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QVERIFY(!cache.isRangeLoaded(rangeStart, rangeEnd));
    QVERIFY(!cache.rangeError(rangeStart, rangeEnd).isEmpty());
}

void CalendarEventCacheTests::
projectionQueryBoundsRetainedRowsAndPreservesConversionErrors()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("projection-capacity.db"));
    const QString connectionName =
        QStringLiteral("calendar-event-cache-capacity-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            );
    constexpr std::size_t maxEvents = ClassMngr::Next::Application::
        kCalendarEventProjectionMaxEvents;
    const QDate eventDate(2026, 7, 15);

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        createCalendarEventsTable(database);
        QVERIFY(database.transaction());

        // SQLite permits an explicit row id of zero. Its blank title would
        // fail projection conversion if invalid ids were not filtered first.
        QSqlQuery invalidIdInsert(database);
        invalidIdInsert.prepare(R"(
            INSERT INTO calendar_events (
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
            )
            VALUES (0, '', 'Other', 'Timed', NULL, 0, ?, '09:00', ?, '10:00')
        )");
        invalidIdInsert.addBindValue(eventDate.toString(Qt::ISODate));
        invalidIdInsert.addBindValue(eventDate.toString(Qt::ISODate));
        QVERIFY(invalidIdInsert.exec());

        QSqlQuery validInsert(database);
        QVERIFY(validInsert.prepare(R"(
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
            VALUES (?, 'Other', 'Timed', NULL, 0, ?, '09:00', ?, '10:00')
        )"));

        const auto insertValidRows =
            [&validInsert, &eventDate](const std::size_t firstIndex,
                                       const std::size_t count)
        {
            for (std::size_t index = firstIndex;
                 index < firstIndex + count;
                 ++index)
            {
                validInsert.bindValue(
                    0,
                    QStringLiteral("valid-%1").arg(
                        static_cast<qulonglong>(index),
                        5,
                        10,
                        QLatin1Char('0')
                        )
                    );
                validInsert.bindValue(1, eventDate.toString(Qt::ISODate));
                validInsert.bindValue(2, eventDate.toString(Qt::ISODate));
                if (!validInsert.exec())
                {
                    return false;
                }
            }

            return true;
        };

        QVERIFY(insertValidRows(0, maxEvents));
        QVERIFY(database.commit());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const auto atLimit = CalendarEventProjectionQuery::loadRange(
        databasePath,
        eventDate,
        eventDate
        );
    QVERIFY(atLimit);
    QCOMPARE(atLimit.value().eventCount(), maxEvents);

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QVERIFY(database.transaction());
        QSqlQuery validInsert(database);
        QVERIFY(validInsert.prepare(R"(
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
            VALUES (?, 'Other', 'Timed', NULL, 0, ?, '09:00', ?, '10:00')
        )"));
        validInsert.addBindValue(QStringLiteral("valid-04096"));
        validInsert.addBindValue(eventDate.toString(Qt::ISODate));
        validInsert.addBindValue(eventDate.toString(Qt::ISODate));
        QVERIFY(validInsert.exec());
        QVERIFY(database.commit());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const auto overLimit = CalendarEventProjectionQuery::loadRange(
        databasePath,
        eventDate,
        eventDate
        );
    QVERIFY(!overLimit);
    QCOMPARE(
        overLimit.error().code,
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );
    QCOMPARE(
        QString::fromStdString(overLimit.error().message),
        QStringLiteral("Calendar event collection exceeds its bounded limit.")
        );

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QVERIFY(database.transaction());
        QSqlQuery malformedInsert(database);
        QVERIFY(malformedInsert.prepare(R"(
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
            VALUES (?, 'Other', 'Timed', NULL, 0, ?, '09:00', ?, '10:00')
        )"));
        malformedInsert.addBindValue(QString(
            static_cast<int>(
                ClassMngr::Next::Application::
                    kCalendarEventSummaryMaxTitleUtf8Bytes + 1
                ),
            QChar('z')
            ));
        malformedInsert.addBindValue(eventDate.toString(Qt::ISODate));
        malformedInsert.addBindValue(eventDate.toString(Qt::ISODate));
        QVERIFY(malformedInsert.exec());
        QVERIFY(database.commit());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    const auto malformedAfterOverflow =
        CalendarEventProjectionQuery::loadRange(
            databasePath,
            eventDate,
            eventDate
            );
    QVERIFY(!malformedAfterOverflow);
    QCOMPARE(
        malformedAfterOverflow.error().code,
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );
    QCOMPARE(
        QString::fromStdString(malformedAfterOverflow.error().message),
        QStringLiteral(
            "A calendar event contains invalid or unbounded projection metadata."
            )
        );
}

void CalendarEventCacheTests::typedDateProjectionPreservesModelParityAndFiltering()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    const QDate eventDate(2026, 7, 15);
    const QString connectionName =
        QStringLiteral("calendar-event-cache-typed-fixture-%1").arg(
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
            QStringLiteral("SNU meeting"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed"),
            QStringLiteral("series-snu"),
            false,
            eventDate,
            QStringLiteral("09:00"),
            eventDate,
            QStringLiteral("10:00")
            );
        insertRawEvent(
            database,
            QStringLiteral("BDG meeting"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed"),
            QStringLiteral("series-bdg"),
            false,
            eventDate,
            QStringLiteral("10:00"),
            eventDate,
            QStringLiteral("11:00")
            );
        insertRawEvent(
            database,
            QStringLiteral("General unknown"),
            QStringLiteral("Other"),
            QStringLiteral("Unknown"),
            QString(),
            false,
            eventDate,
            QString(),
            eventDate,
            QString()
            );
        insertRawEvent(
            database,
            QStringLiteral("All-day event"),
            QStringLiteral("Vacation"),
            QStringLiteral("Timed"),
            QStringLiteral("series-all-day"),
            true,
            eventDate,
            QStringLiteral("13:00"),
            eventDate,
            QStringLiteral("14:00")
            );
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    CalendarEventCache cache;
    cache.setDatabasePath(databasePath);
    cache.requestRange(eventDate, eventDate);
    QTRY_VERIFY_WITH_TIMEOUT(
        cache.isRangeLoaded(eventDate, eventDate),
        5000
        );

    const auto projection = cache.eventProjectionForDate(eventDate);
    QCOMPARE(projection.eventCount(), std::size_t(4));
    QCOMPARE(
        projection.events().at(0).title,
        std::string("All-day event")
        );
    QCOMPARE(
        projection.events().at(1).title,
        std::string("General unknown")
        );
    QCOMPARE(
        projection.events().at(2).title,
        std::string("SNU meeting")
        );
    QCOMPARE(
        projection.events().at(3).title,
        std::string("BDG meeting")
        );
    QVERIFY(projection.events().at(0).allDay);
    QVERIFY(!projection.events().at(1).startTime.has_value());
    QCOMPARE(
        projection.events().at(3).repeatSeriesId,
        std::optional<ClassMngr::Next::Domain::CalendarEventSeriesId>(
            *ClassMngr::Next::Domain::CalendarEventSeriesId::fromString(
                "series-bdg"
                )
            )
        );

    const QList<CalendarEvent> legacyEvents =
        cache.eventsForDate(eventDate);
    QCOMPARE(legacyEvents.size(), projection.eventCount());
    for (int index = 0; index < legacyEvents.size(); ++index)
    {
        QCOMPARE(
            legacyEvents.at(index).title,
            projectionText(projection.events().at(index).title)
            );
    }
    QVERIFY(legacyEvents.at(0).allDay);
    QVERIFY(!legacyEvents.at(1).startTime.isValid());
    QCOMPARE(
        legacyEvents.at(3).repeatSeriesId,
        QStringLiteral("series-bdg")
        );

    CalendarEventModel model(&cache);
    model.setCampusFilter(
        {QStringLiteral("BDG")},
        {QStringLiteral("BDG"), QStringLiteral("SNU")},
        false,
        false
        );
    const QVariantList visible = model.eventsForDate(
        eventDate.year(),
        eventDate.month(),
        eventDate.day()
        );
    QCOMPARE(visible.size(), 3);
    QCOMPARE(
        visible.at(0).toMap().value(QStringLiteral("title")).toString(),
        QStringLiteral("All-day event")
        );
    QCOMPARE(
        visible.at(1).toMap().value(QStringLiteral("title")).toString(),
        QStringLiteral("General unknown")
        );
    QCOMPARE(
        visible.at(2).toMap().value(QStringLiteral("title")).toString(),
        QStringLiteral("BDG meeting")
        );
    QCOMPARE(
        visible.at(2).toMap().value(QStringLiteral("start")).toDateTime(),
        QDateTime(legacyEvents.at(3).startDate, legacyEvents.at(3).startTime)
        );
    QCOMPARE(
        visible.at(2).toMap().value(QStringLiteral("end")).toDateTime(),
        QDateTime(legacyEvents.at(3).endDate, legacyEvents.at(3).endTime)
        );
    QVERIFY(visible.at(0).toMap().value(QStringLiteral("allDay")).toBool());
    QCOMPARE(
        visible.at(1).toMap().value(QStringLiteral("start")).toDateTime(),
        QDateTime(
            legacyEvents.at(1).startDate,
            legacyEvents.at(1).startTime
            )
        );
    QVERIFY(
        !CalendarEventCampusFilter::eventMatchesCampus(
            projection.events().at(2),
            {QStringLiteral("BDG")},
            {QStringLiteral("BDG"), QStringLiteral("SNU")},
            false
            )
        );
}

void CalendarEventCacheTests::typedRangeProjectionPreservesOrderingMetadataAndLegacyParity()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath =
        temporaryDirectory.filePath(QStringLiteral("calendar.db"));
    createDatabase(databasePath);

    const QString connectionName =
        QStringLiteral("calendar-event-cache-range-fixture-%1").arg(
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
            QStringLiteral("SNU meeting"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed"),
            QString(),
            false,
            QDate(2026, 7, 11),
            QStringLiteral("09:00"),
            QDate(2026, 7, 11),
            QStringLiteral("10:00")
            );
        insertRawEvent(
            database,
            QStringLiteral("Repeat unknown-time"),
            QStringLiteral("Holiday"),
            QStringLiteral("Unknown"),
            QStringLiteral("series-repeat"),
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
            QStringLiteral("series-all-day"),
            true,
            QDate(2026, 7, 11),
            QString(),
            QDate(2026, 7, 11),
            QString()
            );
        insertRawEvent(
            database,
            QStringLiteral("BDG meeting"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed"),
            QStringLiteral("series-bdg"),
            false,
            QDate(2026, 7, 12),
            QStringLiteral("10:00"),
            QDate(2026, 7, 12),
            QStringLiteral("11:00")
            );
        insertRawEvent(
            database,
            QStringLiteral("Outside range"),
            QStringLiteral("Other"),
            QStringLiteral("Timed"),
            QString(),
            false,
            QDate(2026, 7, 13),
            QStringLiteral("12:00"),
            QDate(2026, 7, 13),
            QStringLiteral("13:00")
            );
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    CalendarEventCache cache;
    cache.setDatabasePath(databasePath);
    cache.requestRange(
        QDate(2026, 7, 11),
        QDate(2026, 7, 12)
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        cache.isRangeLoaded(
            QDate(2026, 7, 11),
            QDate(2026, 7, 12)
            ),
        5000
        );

    const auto projection = cache.eventProjectionInRange(
        QDate(2026, 7, 11),
        QDate(2026, 7, 12)
        );
    QCOMPARE(projection.eventCount(), std::size_t(4));
    QCOMPARE(
        projection.events().at(0).title,
        std::string("All-day event")
        );
    QCOMPARE(
        projection.events().at(1).title,
        std::string("Repeat unknown-time")
        );
    QCOMPARE(
        projection.events().at(2).title,
        std::string("SNU meeting")
        );
    QCOMPARE(
        projection.events().at(3).title,
        std::string("BDG meeting")
        );
    QCOMPARE(
        projection.events().at(0).startDate,
        std::string("2026-07-11")
        );
    QCOMPARE(
        projection.events().at(3).startDate,
        std::string("2026-07-12")
        );

    const auto& allDay = projection.events().at(0);
    QVERIFY(allDay.allDay);
    QCOMPARE(allDay.eventType, std::string("Vacation"));
    QCOMPARE(
        allDay.repeatSeriesId,
        std::optional<ClassMngr::Next::Domain::CalendarEventSeriesId>(
            *ClassMngr::Next::Domain::CalendarEventSeriesId::fromString(
                "series-all-day"
                )
            )
        );
    QVERIFY(!allDay.startTime);
    QVERIFY(!allDay.endTime);

    const auto& unknown = projection.events().at(1);
    QVERIFY(!unknown.allDay);
    QCOMPARE(
        unknown.timeStatus,
        ClassMngr::Next::Domain::CalendarEventTimeStatus::Unknown
        );
    QCOMPARE(
        unknown.repeatSeriesId,
        std::optional<ClassMngr::Next::Domain::CalendarEventSeriesId>(
            *ClassMngr::Next::Domain::CalendarEventSeriesId::fromString(
                "series-repeat"
                )
            )
        );
    QVERIFY(!unknown.startTime);
    QVERIFY(!unknown.endTime);

    QVERIFY(
        !CalendarEventCampusFilter::eventMatchesCampus(
            projection.events().at(2),
            {QStringLiteral("BDG")},
            {QStringLiteral("BDG"), QStringLiteral("SNU")},
            false
            )
        );
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            projection.events().at(3),
            {QStringLiteral("BDG")},
            {QStringLiteral("BDG"), QStringLiteral("SNU")},
            false
            )
        );

    auto normalizedBoundaryEvent = projection.events().at(3);
    normalizedBoundaryEvent.title = "  meeting s2  ";
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            normalizedBoundaryEvent,
            {QStringLiteral(" s2 ")},
            {QStringLiteral("s2"), QStringLiteral("s20")},
            false
            )
        );
    normalizedBoundaryEvent.title = "  meeting s20  ";
    QVERIFY(
        !CalendarEventCampusFilter::eventMatchesCampus(
            normalizedBoundaryEvent,
            {QStringLiteral(" s2 ")},
            {QStringLiteral("s2"), QStringLiteral("s20")},
            false
            )
        );
    normalizedBoundaryEvent.title = "  meeting xs2y  ";
    QVERIFY(
        CalendarEventCampusFilter::eventMatchesCampus(
            normalizedBoundaryEvent,
            {QStringLiteral("s2")},
            {QStringLiteral("s2")},
            false
            )
        );

    const QList<CalendarEvent> legacyEvents = cache.eventsInRange(
        QDate(2026, 7, 11),
        QDate(2026, 7, 12)
        );
    QCOMPARE(legacyEvents.size(), projection.eventCount());
    for (int index = 0; index < legacyEvents.size(); ++index)
    {
        const auto& summary = projection.events().at(
            static_cast<std::size_t>(index)
            );
        const CalendarEvent& legacy = legacyEvents.at(index);
        QCOMPARE(legacy.title, projectionText(summary.title));
        QCOMPARE(legacy.eventType, projectionText(summary.eventType));
        QCOMPARE(legacy.timeStatus, projectionTimeStatusText(summary.timeStatus));
        QCOMPARE(legacy.startDate, QDate::fromString(
            projectionText(summary.startDate),
            Qt::ISODate
            ));
        QCOMPARE(legacy.endDate, QDate::fromString(
            projectionText(summary.endDate),
            Qt::ISODate
            ));
        QCOMPARE(legacy.allDay, summary.allDay);
        QCOMPARE(
            legacy.repeatSeriesId,
            summary.repeatSeriesId
                ? projectionText(summary.repeatSeriesId->value())
                : QString()
            );

        if (summary.startTime)
        {
            QCOMPARE(
                legacy.startTime,
                QTime::fromString(
                    projectionText(*summary.startTime),
                    QStringLiteral("HH:mm")
                    )
                );
        }
        else
        {
            QVERIFY(!legacy.startTime.isValid());
        }

        if (summary.endTime)
        {
            QCOMPARE(
                legacy.endTime,
                QTime::fromString(
                    projectionText(*summary.endTime),
                    QStringLiteral("HH:mm")
                    )
                );
        }
        else
        {
            QVERIFY(!legacy.endTime.isValid());
        }
    }

    CalendarEventModel model(&cache);
    const QVariantList julyEleventhEvents =
        model.eventsForDate(2026, 7, 11);
    const QVariantList julyTwelfthEvents =
        model.eventsForDate(2026, 7, 12);
    QCOMPARE(julyEleventhEvents.size(), 3);
    QCOMPARE(julyTwelfthEvents.size(), 1);
    QCOMPARE(
        julyEleventhEvents.at(2).toMap()
            .value(QStringLiteral("title")).toString(),
        QStringLiteral("SNU meeting")
        );
    QCOMPARE(
        julyEleventhEvents.at(2).toMap()
            .value(QStringLiteral("start")).toDateTime(),
        QDateTime(legacyEvents.at(2).startDate, legacyEvents.at(2).startTime)
        );
    QCOMPARE(
        julyEleventhEvents.at(2).toMap()
            .value(QStringLiteral("end")).toDateTime(),
        QDateTime(legacyEvents.at(2).endDate, legacyEvents.at(2).endTime)
        );
    QCOMPARE(
        julyTwelfthEvents.at(0).toMap()
            .value(QStringLiteral("title")).toString(),
        QStringLiteral("BDG meeting")
        );
    QCOMPARE(
        julyTwelfthEvents.at(0).toMap()
            .value(QStringLiteral("start")).toDateTime(),
        QDateTime(legacyEvents.at(3).startDate, legacyEvents.at(3).startTime)
        );
    QCOMPARE(
        julyTwelfthEvents.at(0).toMap()
            .value(QStringLiteral("end")).toDateTime(),
        QDateTime(legacyEvents.at(3).endDate, legacyEvents.at(3).endTime)
        );

    QVERIFY(
        cache.eventProjectionInRange(
            QDate(2026, 7, 12),
            QDate(2026, 7, 11)
            ).empty()
        );
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

void CalendarEventCacheTests::
nextEventLookupFailureExposesDiagnosticAndRetryClearsIt()
{
    const auto state = std::make_shared<BlockedQueryState>();
    state->blockQueries = false;
    state->failFirstNextEvent = true;
    CalendarEventCache cache(std::make_shared<BlockedQueryFactory>(state));
    cache.setDatabasePath(QStringLiteral("failed-next-event-calendar.db"));
    const QDate afterDate(2026, 7, 31);
    QSignalSpy foundSpy(&cache, &CalendarEventCache::nextEventMonthFound);
    QSignalSpy failedSpy(&cache, &CalendarEventCache::nextEventMonthFailed);

    cache.requestNextEventMonth(afterDate);
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QCOMPARE(
        cache.nextEventError(afterDate),
        QStringLiteral("injected calendar next event failure")
        );
    QCOMPARE(failedSpy.size(), 1);
    QCOMPARE(failedSpy.first().first().toDate(), afterDate);
    QCOMPARE(foundSpy.size(), 0);
    QCOMPARE(state->calls.load(), 1);

    cache.requestNextEventMonth(afterDate);
    QVERIFY(cache.nextEventError(afterDate).isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QCOMPARE(foundSpy.size(), 1);
    QCOMPARE(foundSpy.first().first().toDate(), QDate(2026, 8, 10));
    QVERIFY(cache.nextEventError(afterDate).isEmpty());
    QCOMPARE(state->calls.load(), 2);
}

void CalendarEventCacheTests::
nextEventLookupFailureDiagnosticClearsOnInvalidation()
{
    const auto state = std::make_shared<BlockedQueryState>();
    state->blockQueries = false;
    state->failFirstNextEvent = true;
    CalendarEventCache cache(std::make_shared<BlockedQueryFactory>(state));
    cache.setDatabasePath(QStringLiteral("invalidated-next-event-calendar.db"));
    const QDate afterDate(2026, 7, 31);

    cache.requestNextEventMonth(afterDate);
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QVERIFY(!cache.nextEventError(afterDate).isEmpty());

    cache.invalidate();
    QVERIFY(cache.nextEventError(afterDate).isEmpty());
}

void CalendarEventCacheTests::rangeFailureExposesDiagnosticAndRetryClearsIt()
{
    const auto state = std::make_shared<BlockedQueryState>();
    state->blockQueries = false;
    state->failAllRanges = true;
    CalendarEventCache cache(std::make_shared<BlockedQueryFactory>(state));
    cache.setDatabasePath(QStringLiteral("failed-calendar.db"));
    const QDate start(2026, 7, 1);
    const QDate end(2026, 7, 31);

    cache.requestRange(start, end);
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QVERIFY(!cache.isRangeLoaded(start, end));
    QCOMPARE(cache.loadedRangeCount(), 0);
    QCOMPARE(
        cache.rangeError(start, end),
        QStringLiteral("injected calendar range failure")
        );
    QCOMPARE(state->calls.load(), 1);

    state->failAllRanges = false;
    cache.requestRange(start, end);
    QCOMPARE(cache.rangeError(start, end), QString());
    QTRY_VERIFY_WITH_TIMEOUT(cache.isRangeLoaded(start, end), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QCOMPARE(cache.rangeError(start, end), QString());
    QCOMPARE(cache.eventCount(), 1);
    QCOMPARE(state->calls.load(), 2);
}

void CalendarEventCacheTests::rangeFailureDiagnosticFollowsRetainedRanges()
{
    const auto state = std::make_shared<BlockedQueryState>();
    state->blockQueries = false;
    state->failAllRanges = true;
    CalendarEventCache cache(std::make_shared<BlockedQueryFactory>(state));
    cache.setDatabasePath(QStringLiteral("retained-failure-calendar.db"));

    const QDate start(2026, 7, 1);
    const QDate end(2026, 7, 31);
    cache.setRetainedRanges({{start, end}});
    cache.requestRange(start, end);
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QCOMPARE(
        cache.rangeError(start, end),
        QStringLiteral("injected calendar range failure")
        );

    cache.setRetainedRanges({{QDate(2026, 7, 10), QDate(2026, 7, 20)}});
    QCOMPARE(
        cache.rangeError(start, end),
        QStringLiteral("injected calendar range failure")
        );
    QVERIFY(
        cache.rangeError(QDate(2026, 7, 10), QDate(2026, 7, 20)).isEmpty()
        );

    cache.setRetainedRanges({{QDate(2026, 8, 1), QDate(2026, 8, 31)}});
    QVERIFY(cache.rangeError(start, end).isEmpty());
}

void CalendarEventCacheTests::staleGenerationRangeFailureDoesNotLeak()
{
    const auto state = std::make_shared<BlockedQueryState>();
    state->failFirstRange = true;
    CalendarEventCache cache(std::make_shared<BlockedQueryFactory>(state));
    ResumeQueriesOnExit resumeOnExit{state};
    cache.setDatabasePath(QStringLiteral("stale-failure-calendar.db"));
    const QDate start(2026, 7, 1);
    const QDate end(2026, 7, 31);

    cache.requestRange(start, end);
    QVERIFY(state->entered.tryAcquire(1, 5000));
    cache.invalidate();
    cache.requestRange(start, end);

    state->resume.release();
    QTRY_VERIFY_WITH_TIMEOUT(state->entered.available() == 1, 5000);
    QVERIFY(state->entered.tryAcquire());
    QVERIFY(cache.rangeError(start, end).isEmpty());
    QVERIFY(!cache.isRangeLoaded(start, end));
    QVERIFY(cache.isLoading());

    state->resume.release();
    QTRY_VERIFY_WITH_TIMEOUT(cache.isRangeLoaded(start, end), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QVERIFY(cache.rangeError(start, end).isEmpty());
    QCOMPARE(cache.eventsForDate(QDate(2026, 7, 10)).first().title,
        QStringLiteral("Fresh generation"));
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

void CalendarEventCacheTests::invalidatedActiveRangeDoesNotSuppressSameRangeReissue()
{
    const auto state = std::make_shared<BlockedQueryState>();
    CalendarEventCache cache(std::make_shared<BlockedQueryFactory>(state));
    ResumeQueriesOnExit resumeOnExit{state};
    CalendarEventModel model(&cache);
    cache.setDatabasePath(QStringLiteral("blocked-calendar.db"));
    const QDate start(2026, 7, 1);
    const QDate end(2026, 7, 31);
    cache.requestRange(start, end);
    QVERIFY(state->entered.tryAcquire(1, 5000));
    cache.requestRange(start, end); // Same-generation active duplicate.
    cache.invalidate();
    QSignalSpy changes(&cache, &CalendarEventCache::cacheChanged);
    cache.requestRange(start, end);
    cache.requestRange(start, end); // Same-generation queued duplicate.
    QCOMPARE(state->calls.load(), 1);

    state->resume.release();
    QTRY_VERIFY_WITH_TIMEOUT(state->entered.available() == 1, 5000);
    QVERIFY(state->entered.tryAcquire());
    QCOMPARE(state->calls.load(), 2);
    QCOMPARE(changes.size(), 0);
    QCOMPARE(cache.eventCount(), 0);
    QVERIFY(!cache.isRangeLoaded(start, end));
    QVERIFY(model.eventsForDate(2026, 7, 10).isEmpty());
    QVERIFY(cache.isLoading());

    state->resume.release();
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QVERIFY(cache.isRangeLoaded(start, end));
    QCOMPARE(changes.size(), 1);
    QCOMPARE(cache.eventsForDate(QDate(2026, 7, 10)).first().title,
        QStringLiteral("Fresh generation"));
    QCOMPARE(model.eventsForDate(2026, 7, 10).size(), 1);
    QCOMPARE(state->calls.load(), 2);
}

void CalendarEventCacheTests::invalidatedActiveNextEventLookupDoesNotSuppressReissue()
{
    const auto state = std::make_shared<BlockedQueryState>();
    state->failFirstNextEvent = true;
    CalendarEventCache cache(std::make_shared<BlockedQueryFactory>(state));
    ResumeQueriesOnExit resumeOnExit{state};
    cache.setDatabasePath(QStringLiteral("blocked-calendar.db"));
    const QDate after(2026, 7, 1);
    QSignalSpy found(&cache, &CalendarEventCache::nextEventMonthFound);
    cache.requestNextEventMonth(after);
    QVERIFY(state->entered.tryAcquire(1, 5000));
    cache.requestNextEventMonth(after);
    cache.invalidate();
    cache.requestNextEventMonth(after);
    cache.requestNextEventMonth(after);
    QCOMPARE(state->calls.load(), 1);

    state->resume.release();
    QTRY_VERIFY_WITH_TIMEOUT(state->entered.available() == 1, 5000);
    QVERIFY(state->entered.tryAcquire());
    QCOMPARE(state->calls.load(), 2);
    QCOMPARE(found.size(), 0); // Old generation cannot advance next-ten progress.
    QVERIFY(cache.nextEventError(after).isEmpty());
    QVERIFY(cache.isLoading());
    state->resume.release();
    QTRY_VERIFY_WITH_TIMEOUT(!cache.isLoading(), 5000);
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().first().toDate(), QDate(2026, 8, 10));
    QVERIFY(cache.nextEventError(after).isEmpty());
    QCOMPARE(state->calls.load(), 2);
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
    const auto projection = cache.eventProjectionInRange(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );
    QCOMPARE(projection.eventCount(), std::size_t(2));
    QCOMPARE(
        projection.events().at(0).title,
        std::string("Cached event")
        );
    QCOMPARE(
        projection.events().at(1).title,
        std::string("Three-day event")
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

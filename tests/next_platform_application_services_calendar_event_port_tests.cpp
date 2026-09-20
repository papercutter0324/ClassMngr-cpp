#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/platform/application_services_calendar_event_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Platform::ApplicationServicesCalendarEventPort;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("calendar-port-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
{
    return services.openDatabase(databasePath(directory)).has_value();
}

CalendarEvent makeEvent(
    const QString& title,
    const QDate& startDate,
    const QDate& endDate
    )
{
    CalendarEvent event;
    event.title = title;
    event.startDate = startDate;
    event.endDate = endDate;
    return event;
}

int saveEvent(
    CalendarService& service,
    const CalendarEvent& event
    )
{
    const auto saved = service.saveEvent(event);
    return saved ? *saved : -1;
}

CalendarEventId calendarEventId(const int id)
{
    return *CalendarEventId::fromString(std::to_string(id));
}

void verifyFailure(
    const Domain::Result<CalendarEventProjection>& result,
    const ErrorCode expectedCode
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, expectedCode);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

} // namespace

class NextPlatformApplicationServicesCalendarEventPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void projectsOverlappingRangeAsOwnedTypedMetadata();
    void preservesLegacyTitleSurroundingSpaces();
    void preservesAllDayAndUnknownTimePolicy();
    void reportsUnavailableAndInvalidRangesStructurally();
    void rejectsPartialSourceTimesAndProjectionOverflow();
    void rejectsMalformedRepeatSeriesMetadataStructurally();
    void boundaryIsTypedAndDoesNotExposeLegacyOwnership();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesCalendarEventPortTests::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
projectsOverlappingRangeAsOwnedTypedMetadata()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);
    QVERIFY(legacyService->isAvailable());

    CalendarEvent overlapsStart = makeEvent(
        QStringLiteral("Overlap start"),
        QDate(2026, 6, 30),
        QDate(2026, 7, 2)
        );
    overlapsStart.startTime = QTime(8, 5);
    overlapsStart.endTime = QTime(9, 35);
    overlapsStart.eventType = QStringLiteral(" Workshop ");
    overlapsStart.timeStatus = QStringLiteral(" Timed ");
    overlapsStart.repeatSeriesId = QStringLiteral(" series-july ");

    CalendarEvent inside = makeEvent(
        QStringLiteral("회의 일정"),
        QDate(2026, 7, 10),
        QDate(2026, 7, 10)
        );
    inside.startTime = QTime(10, 0);
    inside.endTime = QTime(11, 30);
    inside.eventType = QStringLiteral("Meeting");
    inside.timeStatus = QStringLiteral("Timed");
    inside.repeatSeriesId = QStringLiteral("series-july");

    CalendarEvent overlapsEnd = makeEvent(
        QStringLiteral("Overlap end"),
        QDate(2026, 7, 31),
        QDate(2026, 8, 2)
        );
    overlapsEnd.startTime = QTime(12, 0);
    overlapsEnd.endTime = QTime(13, 0);
    overlapsEnd.eventType = QStringLiteral("Holiday");
    overlapsEnd.timeStatus = QStringLiteral("Timed");

    CalendarEvent outside = makeEvent(
        QStringLiteral("Outside"),
        QDate(2026, 8, 3),
        QDate(2026, 8, 3)
        );
    outside.startTime = QTime(9, 0);
    outside.endTime = QTime(10, 0);

    const int overlapsStartId = saveEvent(*legacyService, overlapsStart);
    const int insideId = saveEvent(*legacyService, inside);
    const int overlapsEndId = saveEvent(*legacyService, overlapsEnd);
    const int outsideId = saveEvent(*legacyService, outside);
    QVERIFY(overlapsStartId > 0);
    QVERIFY(insideId > 0);
    QVERIFY(overlapsEndId > 0);
    QVERIFY(outsideId > 0);

    ApplicationServicesCalendarEventPort port(services);
    const auto result = port.projection(
        QDate(2026, 7, 1),
        QDate(2026, 7, 31)
        );

    QVERIFY(result);
    QCOMPARE(result.value().eventCount(), std::size_t(3));
    const auto& events = result.value().events();
    QCOMPARE(events.size(), std::size_t(3));
    QCOMPARE(events.at(0).id.value(), std::to_string(overlapsStartId));
    QCOMPARE(events.at(0).title, std::string("Overlap start"));
    QCOMPARE(events.at(0).startDate, std::string("2026-06-30"));
    QCOMPARE(events.at(0).endDate, std::string("2026-07-02"));
    QCOMPARE(events.at(0).startTime.value(), std::string("08:05"));
    QCOMPARE(events.at(0).endTime.value(), std::string("09:35"));
    QCOMPARE(events.at(0).eventType, std::string("Workshop"));
    QCOMPARE(events.at(0).timeStatus, std::string("Timed"));
    QVERIFY(events.at(0).repeatSeriesId.has_value());
    QCOMPARE(events.at(0).repeatSeriesId.value(), std::string("series-july"));
    QCOMPARE(events.at(0).order, std::int32_t(0));
    QVERIFY(!events.at(0).classId.has_value());
    QVERIFY(!events.at(0).campusId.has_value());
    QVERIFY(events.at(0).location.empty());
    QVERIFY(events.at(0).notes.empty());

    QCOMPARE(events.at(1).id.value(), std::to_string(insideId));
    QCOMPARE(events.at(1).title, QStringLiteral("회의 일정").toUtf8().toStdString());
    QCOMPARE(events.at(1).startDate, std::string("2026-07-10"));
    QCOMPARE(events.at(1).endDate, std::string("2026-07-10"));
    QCOMPARE(events.at(1).startTime.value(), std::string("10:00"));
    QCOMPARE(events.at(1).endTime.value(), std::string("11:30"));
    QCOMPARE(events.at(1).eventType, std::string("Meeting"));
    QCOMPARE(events.at(1).timeStatus, std::string("Timed"));
    QCOMPARE(events.at(1).repeatSeriesId.value(), std::string("series-july"));
    QCOMPARE(events.at(1).order, std::int32_t(1));

    QCOMPARE(events.at(2).id.value(), std::to_string(overlapsEndId));
    QCOMPARE(events.at(2).title, std::string("Overlap end"));
    QCOMPARE(events.at(2).eventType, std::string("Holiday"));
    QCOMPARE(events.at(2).timeStatus, std::string("Timed"));
    QVERIFY(!events.at(2).repeatSeriesId.has_value());
    QCOMPARE(events.at(2).order, std::int32_t(2));
    QVERIFY(!result.value().findEvent(calendarEventId(outsideId)).has_value());

    const auto copied = result.value().findEvent(calendarEventId(insideId));
    QVERIFY(copied.has_value());
    QCOMPARE(copied->title, QStringLiteral("회의 일정").toUtf8().toStdString());

    services.closeDatabase();
    QCOMPARE(
        result.value().findEvent(calendarEventId(insideId))->startDate,
        std::string("2026-07-10")
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
preservesLegacyTitleSurroundingSpaces()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    query.prepare(QStringLiteral(
        "INSERT INTO calendar_events ("
        "title, event_type, time_status, repeat_series_id, all_day, "
        "start_date, start_time, end_date, end_time) "
        "VALUES (?, 'Other', 'Timed', NULL, 0, ?, ?, ?, ?)"
        ));
    query.addBindValue(QStringLiteral("  Legacy title  "));
    query.addBindValue(QStringLiteral("2026-08-15"));
    query.addBindValue(QStringLiteral("09:00"));
    query.addBindValue(QStringLiteral("2026-08-15"));
    query.addBindValue(QStringLiteral("10:00"));
    QVERIFY(query.exec());

    ApplicationServicesCalendarEventPort port(services);
    const auto result = port.projection(
        QDate(2026, 8, 15),
        QDate(2026, 8, 15)
        );
    QVERIFY(result);
    QCOMPARE(result.value().eventCount(), std::size_t(1));
    QCOMPARE(
        result.value().events().front().title,
        QStringLiteral("  Legacy title  ").toUtf8().toStdString()
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
preservesAllDayAndUnknownTimePolicy()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    CalendarEvent allDay = makeEvent(
        QStringLiteral("All day"),
        QDate(2026, 9, 20),
        QDate(2026, 9, 21)
        );
    // The legacy dialog stores all-day events with sentinel times.
    allDay.allDay = true;
    allDay.startTime = QTime(0, 0);
    allDay.endTime = QTime(23, 59);
    allDay.eventType = QStringLiteral(" Vacation ");
    allDay.timeStatus = QStringLiteral("Timed");
    allDay.repeatSeriesId = QStringLiteral(" series-all-day ");

    CalendarEvent unknownTime = makeEvent(
        QStringLiteral("Unknown time"),
        QDate(2026, 9, 20),
        QDate(2026, 9, 20)
        );
    unknownTime.timeStatus = QStringLiteral("Unknown");
    unknownTime.eventType = QStringLiteral(" Holiday ");
    unknownTime.repeatSeriesId = QStringLiteral(" \t ");

    const int allDayId = saveEvent(*legacyService, allDay);
    const int unknownTimeId = saveEvent(*legacyService, unknownTime);
    QVERIFY(allDayId > 0);
    QVERIFY(unknownTimeId > 0);

    ApplicationServicesCalendarEventPort port(services);
    const auto result = port.projection(
        QDate(2026, 9, 20),
        QDate(2026, 9, 20)
        );
    QVERIFY(result);

    const auto allDayProjection = result.value().findEvent(
        calendarEventId(allDayId)
        );
    QVERIFY(allDayProjection.has_value());
    QVERIFY(allDayProjection->allDay);
    QVERIFY(!allDayProjection->startTime.has_value());
    QVERIFY(!allDayProjection->endTime.has_value());
    QCOMPARE(allDayProjection->eventType, std::string("Vacation"));
    QCOMPARE(allDayProjection->timeStatus, std::string("Timed"));
    QCOMPARE(
        allDayProjection->repeatSeriesId.value(),
        std::string("series-all-day")
        );

    const auto unknownProjection = result.value().findEvent(
        calendarEventId(unknownTimeId)
        );
    QVERIFY(unknownProjection.has_value());
    QVERIFY(!unknownProjection->allDay);
    QVERIFY(!unknownProjection->startTime.has_value());
    QVERIFY(!unknownProjection->endTime.has_value());
    QCOMPARE(unknownProjection->eventType, std::string("Holiday"));
    QCOMPARE(unknownProjection->timeStatus, std::string("Unknown"));
    QVERIFY(!unknownProjection->repeatSeriesId.has_value());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableAndInvalidRangesStructurally()
{
    ApplicationServices unavailableServices;
    ApplicationServicesCalendarEventPort unavailablePort(
        unavailableServices
        );
    verifyFailure(
        unavailablePort.projection(
            QDate(2026, 9, 20),
            QDate(2026, 9, 21)
            ),
        ErrorCode::NotFound
        );

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesCalendarEventPort port(services);
    verifyFailure(
        port.projection(QDate(), QDate(2026, 9, 21)),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.projection(QDate(2026, 9, 21), QDate(2026, 9, 20)),
        ErrorCode::InvalidInput
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
rejectsPartialSourceTimesAndProjectionOverflow()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    // The service validator correctly rejects partial times on writes. Insert
    // a malformed persisted row directly so the read adapter's validation is
    // exercised against the legacy shape it must defend against.
    int partialId = -1;
    {
        QSqlQuery query(
            services.dataService()->databaseSession()->database()
            );
        query.prepare(QStringLiteral(
            "INSERT INTO calendar_events ("
            "title, event_type, time_status, repeat_series_id, all_day, "
            "start_date, start_time, end_date, end_time) "
            "VALUES (?, 'Other', 'Timed', NULL, 0, ?, ?, ?, NULL)"
            ));
        query.addBindValue(QStringLiteral("Partial time"));
        query.addBindValue(QStringLiteral("2026-10-01"));
        query.addBindValue(QStringLiteral("09:00"));
        query.addBindValue(QStringLiteral("2026-10-01"));
        QVERIFY(query.exec());
        partialId = query.lastInsertId().toInt();
    }
    QVERIFY(partialId > 0);

    ApplicationServicesCalendarEventPort port(services);
    verifyFailure(
        port.projection(QDate(2026, 10, 1), QDate(2026, 10, 1)),
        ErrorCode::InvalidInput
        );

    services.closeDatabase();
    QVERIFY(openDatabase(services, m_directory));
    legacyService = services.calendarService();
    QVERIFY(legacyService);

    QList<CalendarEvent> overflowEvents;
    overflowEvents.reserve(
        static_cast<int>(kCalendarEventProjectionMaxEvents + 1)
        );
    for (std::size_t index = 0;
         index < kCalendarEventProjectionMaxEvents + 1;
         ++index)
    {
        CalendarEvent event = makeEvent(
            QStringLiteral("Overflow %1").arg(
                static_cast<qulonglong>(index)
                ),
            QDate(2026, 11, 1),
            QDate(2026, 11, 1)
            );
        event.startTime = QTime(9, 0);
        event.endTime = QTime(10, 0);
        overflowEvents.append(std::move(event));
    }

    const auto saved = legacyService->saveEvents(overflowEvents);
    QVERIFY(saved);
    QCOMPARE(
        saved->size(),
        static_cast<qsizetype>(kCalendarEventProjectionMaxEvents + 1)
        );

    const auto overflow = port.projection(
        QDate(2026, 11, 1),
        QDate(2026, 11, 1)
        );
    verifyFailure(overflow, ErrorCode::InvalidInput);
    QVERIFY(
        overflow.error().message.find("bounded") != std::string::npos
        || overflow.error().message.find("capacity") != std::string::npos
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
rejectsMalformedRepeatSeriesMetadataStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    query.prepare(QStringLiteral(
        "INSERT INTO calendar_events ("
        "title, event_type, time_status, repeat_series_id, all_day, "
        "start_date, start_time, end_date, end_time) "
        "VALUES (?, 'Other', 'Timed', ?, 0, ?, ?, ?, ?)"
        ));
    query.addBindValue(QStringLiteral("Oversized repeat series"));
    query.addBindValue(QString(
        static_cast<int>(kCalendarEventSummaryMaxRepeatSeriesIdLength + 1),
        QChar('r')
        ));
    query.addBindValue(QStringLiteral("2026-10-15"));
    query.addBindValue(QStringLiteral("09:00"));
    query.addBindValue(QStringLiteral("2026-10-15"));
    query.addBindValue(QStringLiteral("10:00"));
    QVERIFY(query.exec());

    ApplicationServicesCalendarEventPort port(services);
    const auto result = port.projection(
        QDate(2026, 10, 15),
        QDate(2026, 10, 15)
        );
    verifyFailure(result, ErrorCode::InvalidInput);
    QVERIFY(
        result.error().message.find("repeat-series") != std::string::npos
        || result.error().message.find("unbounded") != std::string::npos
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
boundaryIsTypedAndDoesNotExposeLegacyOwnership()
{
    using Port = ApplicationServicesCalendarEventPort;
    using ProjectionResult = decltype(
        std::declval<const Port&>().projection(
            std::declval<const QDate&>(),
            std::declval<const QDate&>()
            )
        );

    static_assert(std::is_same_v<
        ProjectionResult,
        Domain::Result<CalendarEventProjection>
        >);
    static_assert(!std::is_copy_constructible_v<Port>);
    static_assert(!std::is_move_constructible_v<Port>);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventSummary>().eventType),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventSummary>().timeStatus),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventSummary>().repeatSeriesId),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventProjection>().events()),
        const std::vector<CalendarEventSummary>&
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<const CalendarEventProjection>().events())
        >);

    QVERIFY(true);
}

QTEST_MAIN(NextPlatformApplicationServicesCalendarEventPortTests)

#include "next_platform_application_services_calendar_event_port_tests.moc"

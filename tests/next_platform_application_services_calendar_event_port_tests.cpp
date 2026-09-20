#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/calendar_event_delete_port.h"
#include "next/application/calendar_event_series_delete_port.h"
#include "next/platform/application_services_calendar_event_delete_port.h"
#include "next/platform/application_services_calendar_event_series_delete_port.h"
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
using ClassMngr::Next::Platform::ApplicationServicesCalendarEventDeletePort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventSeriesDeletePort;
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

template <typename Value>
void verifyFailure(
    const Domain::Result<Value>& result,
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
    void projectsByIdAsOwnedTypedMetadata();
    void deletesValidTypedEvent();
    void reportsInvalidDeleteIdStructurally();
    void reportsUnavailableDeleteServiceStructurally();
    void reportsDeleteServiceFailureStructurally();
    void deletesValidRepeatSeriesSuffix();
    void reportsInvalidRepeatSeriesDeleteRequestStructurally();
    void reportsUnavailableRepeatSeriesDeleteServiceStructurally();
    void reportsRepeatSeriesDeleteServiceFailureStructurally();
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
projectsByIdAsOwnedTypedMetadata()
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
        "VALUES (?, ?, ?, ?, 0, ?, ?, ?, ?)"
        ));
    query.addBindValue(QStringLiteral("Activation event"));
    query.addBindValue(QStringLiteral("Workshop"));
    query.addBindValue(QStringLiteral("Unconfirmed"));
    query.addBindValue(QStringLiteral("series-activation"));
    query.addBindValue(QStringLiteral("2026-12-24"));
    query.addBindValue(QStringLiteral("08:05"));
    query.addBindValue(QStringLiteral("2026-12-26"));
    query.addBindValue(QStringLiteral("09:35"));
    QVERIFY(query.exec());
    const int eventId = query.lastInsertId().toInt();
    QVERIFY(eventId > 0);

    ApplicationServicesCalendarEventPort port(services);
    const auto result = port.projectionById(eventId);

    QVERIFY(result);
    const CalendarEventSummary& projected = result.value();
    QCOMPARE(projected.id.value(), std::to_string(eventId));
    QCOMPARE(projected.title, std::string("Activation event"));
    QCOMPARE(projected.eventType, std::string("Workshop"));
    QCOMPARE(projected.timeStatus, std::string("Unconfirmed"));
    QCOMPARE(projected.repeatSeriesId, std::optional<std::string>(
        std::string("series-activation")
        ));
    QVERIFY(!projected.allDay);
    QCOMPARE(projected.startDate, std::string("2026-12-24"));
    QCOMPARE(projected.endDate, std::string("2026-12-26"));
    QCOMPARE(projected.startTime, std::optional<std::string>(
        std::string("08:05")
        ));
    QCOMPARE(projected.endTime, std::optional<std::string>(
        std::string("09:35")
        ));
    QCOMPARE(projected.order, std::int32_t(0));
    QVERIFY(!projected.classId.has_value());
    QVERIFY(!projected.campusId.has_value());
    QVERIFY(projected.location.empty());
    QVERIFY(projected.notes.empty());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
deletesValidTypedEvent()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    CalendarEvent event = makeEvent(
        QStringLiteral("Delete me"),
        QDate(2026, 12, 1),
        QDate(2026, 12, 1)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = saveEvent(*legacyService, event);
    QVERIFY(eventId > 0);

    ApplicationServicesCalendarEventDeletePort port(services);
    const auto deleted = port.deleteEvent(calendarEventId(eventId));

    QVERIFY(deleted);
    QVERIFY(!legacyService->event(eventId));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsInvalidDeleteIdStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesCalendarEventDeletePort port(services);
    verifyFailure(
        port.deleteEvent(calendarEventId(0)),
        ErrorCode::InvalidInput
        );

    const auto malformedId = CalendarEventId::fromString(
        "not-an-integer"
        );
    QVERIFY(malformedId.has_value());
    verifyFailure(
        port.deleteEvent(*malformedId),
        ErrorCode::InvalidInput
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableDeleteServiceStructurally()
{
    ApplicationServices services;
    ApplicationServicesCalendarEventDeletePort port(services);

    verifyFailure(
        port.deleteEvent(calendarEventId(1)),
        ErrorCode::NotFound
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsDeleteServiceFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    CalendarEvent event = makeEvent(
        QStringLiteral("Delete failure"),
        QDate(2026, 12, 2),
        QDate(2026, 12, 2)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = saveEvent(*legacyService, event);
    QVERIFY(eventId > 0);

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_delete "
        "BEFORE DELETE ON calendar_events "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar delete failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventDeletePort port(services);
    const auto deleted = port.deleteEvent(calendarEventId(eventId));

    verifyFailure(deleted, ErrorCode::Technical);
    QVERIFY(
        deleted.error().message.find("Deleting calendar event")
            != std::string::npos
        );
    QVERIFY(legacyService->event(eventId));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
deletesValidRepeatSeriesSuffix()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    const QString repeatSeriesId = QStringLiteral("series-delete-suffix");
    QList<int> eventIds;
    for (const QDate& startDate : {
             QDate(2026, 12, 1),
             QDate(2026, 12, 8),
             QDate(2026, 12, 15)
         })
    {
        CalendarEvent event = makeEvent(
            QStringLiteral("Series event"),
            startDate,
            startDate
            );
        event.startTime = QTime(9, 0);
        event.endTime = QTime(10, 0);
        event.repeatSeriesId = repeatSeriesId;
        const int eventId = saveEvent(*legacyService, event);
        QVERIFY(eventId > 0);
        eventIds.append(eventId);
    }

    ApplicationServicesCalendarEventSeriesDeletePort port(services);
    const CalendarEventSeriesDeleteRequest request{
        repeatSeriesId.toUtf8().toStdString(),
        QStringLiteral("2026-12-08").toStdString()
    };
    const auto deleted = port.deleteRepeatSeriesFromDate(request);

    QVERIFY(deleted);
    QVERIFY(legacyService->event(eventIds.at(0)));
    QVERIFY(!legacyService->event(eventIds.at(1)));
    QVERIFY(!legacyService->event(eventIds.at(2)));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsInvalidRepeatSeriesDeleteRequestStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesCalendarEventSeriesDeletePort port(services);
    verifyFailure(
        port.deleteRepeatSeriesFromDate({
            "   ",
            "2026-12-08"
        }),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.deleteRepeatSeriesFromDate({
            "series-delete-invalid",
            "2026-13-08"
        }),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.deleteRepeatSeriesFromDate({
            std::string(
                kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength + 1,
                'r'
                ),
            "2026-12-08"
        }),
        ErrorCode::InvalidInput
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableRepeatSeriesDeleteServiceStructurally()
{
    ApplicationServices services;
    ApplicationServicesCalendarEventSeriesDeletePort port(services);

    verifyFailure(
        port.deleteRepeatSeriesFromDate({
            "series-delete-unavailable",
            "2026-12-08"
        }),
        ErrorCode::NotFound
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsRepeatSeriesDeleteServiceFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    CalendarEvent event = makeEvent(
        QStringLiteral("Repeat delete failure"),
        QDate(2026, 12, 2),
        QDate(2026, 12, 2)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    event.repeatSeriesId = QStringLiteral("series-delete-failure");
    const int eventId = saveEvent(*legacyService, event);
    QVERIFY(eventId > 0);

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_repeat_delete "
        "BEFORE DELETE ON calendar_events "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar repeat delete failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventSeriesDeletePort port(services);
    const auto deleted = port.deleteRepeatSeriesFromDate({
        "series-delete-failure",
        "2026-12-02"
    });

    verifyFailure(deleted, ErrorCode::Technical);
    QVERIFY(
        deleted.error().message.find("Deleting calendar repeat series")
            != std::string::npos
        );
    QVERIFY(legacyService->event(eventId));
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

    const auto allDayById = port.projectionById(allDayId);
    QVERIFY(allDayById);
    QCOMPARE(allDayById.value().id.value(), std::to_string(allDayId));
    QCOMPARE(allDayById.value().startDate, std::string("2026-09-20"));
    QCOMPARE(allDayById.value().endDate, std::string("2026-09-21"));
    QVERIFY(allDayById.value().allDay);
    QVERIFY(!allDayById.value().startTime.has_value());
    QVERIFY(!allDayById.value().endTime.has_value());
    QCOMPARE(allDayById.value().eventType, std::string("Vacation"));
    QCOMPARE(allDayById.value().timeStatus, std::string("Timed"));
    QCOMPARE(
        allDayById.value().repeatSeriesId,
        std::optional<std::string>(std::string("series-all-day"))
        );

    const auto unknownById = port.projectionById(unknownTimeId);
    QVERIFY(unknownById);
    QVERIFY(!unknownById.value().allDay);
    QVERIFY(!unknownById.value().startTime.has_value());
    QVERIFY(!unknownById.value().endTime.has_value());
    QCOMPARE(unknownById.value().eventType, std::string("Holiday"));
    QCOMPARE(unknownById.value().timeStatus, std::string("Unknown"));
    QVERIFY(!unknownById.value().repeatSeriesId.has_value());
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
    verifyFailure(
        unavailablePort.projectionById(1),
        ErrorCode::NotFound
        );

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesCalendarEventPort port(services);
    verifyFailure(
        port.projectionById(0),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.projectionById(999999),
        ErrorCode::NotFound
        );
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
    const int eventId = query.lastInsertId().toInt();
    QVERIFY(eventId > 0);

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

    const auto byIdResult = port.projectionById(eventId);
    verifyFailure(byIdResult, ErrorCode::InvalidInput);
    QVERIFY(
        byIdResult.error().message.find("repeat-series") != std::string::npos
        || byIdResult.error().message.find("unbounded") != std::string::npos
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
boundaryIsTypedAndDoesNotExposeLegacyOwnership()
{
    using Port = ApplicationServicesCalendarEventPort;
    using DeletePort = CalendarEventDeletePort;
    using SeriesDeletePort = CalendarEventSeriesDeletePort;
    using ProjectionResult = decltype(
        std::declval<const Port&>().projection(
            std::declval<const QDate&>(),
            std::declval<const QDate&>()
            )
        );
    using ByIdResult = decltype(
        std::declval<const Port&>().projectionById(1)
        );
    using DeleteResult = decltype(
        std::declval<DeletePort&>().deleteEvent(
            std::declval<const CalendarEventId&>()
            )
        );
    using SeriesDeleteResult = decltype(
        std::declval<SeriesDeletePort&>().deleteRepeatSeriesFromDate(
            std::declval<const CalendarEventSeriesDeleteRequest&>()
            )
        );

    static_assert(std::is_same_v<
        ProjectionResult,
        Domain::Result<CalendarEventProjection>
        >);
    static_assert(std::is_same_v<
        ByIdResult,
        Domain::Result<CalendarEventSummary>
        >);
    static_assert(std::is_same_v<
        DeleteResult,
        Domain::Result<void>
        >);
    static_assert(std::is_same_v<
        SeriesDeleteResult,
        Domain::Result<void>
        >);
    static_assert(std::is_base_of_v<
        DeletePort,
        ApplicationServicesCalendarEventDeletePort
        >);
    static_assert(!std::is_copy_constructible_v<
        ApplicationServicesCalendarEventDeletePort
        >);
    static_assert(!std::is_move_constructible_v<
        ApplicationServicesCalendarEventDeletePort
        >);
    static_assert(std::is_base_of_v<
        SeriesDeletePort,
        ApplicationServicesCalendarEventSeriesDeletePort
        >);
    static_assert(!std::is_copy_constructible_v<
        ApplicationServicesCalendarEventSeriesDeletePort
        >);
    static_assert(!std::is_move_constructible_v<
        ApplicationServicesCalendarEventSeriesDeletePort
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesDeleteRequest>().repeatSeriesId),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesDeleteRequest>().startDate),
        std::string
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

#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/calendar_event_delete_port.h"
#include "next/application/calendar_event_save_port.h"
#include "next/application/calendar_event_series_create_port.h"
#include "next/application/calendar_event_series_edit_port.h"
#include "next/application/calendar_event_series_delete_port.h"
#include "next/platform/application_services_calendar_event_delete_port.h"
#include "next/platform/application_services_calendar_event_save_port.h"
#include "next/platform/application_services_calendar_event_series_create_port.h"
#include "next/platform/application_services_calendar_event_series_edit_port.h"
#include "next/platform/application_services_calendar_event_series_delete_port.h"
#include "next/platform/application_services_calendar_event_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Platform::ApplicationServicesCalendarEventDeletePort;
using ClassMngr::Next::Platform::ApplicationServicesCalendarEventSavePort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventSeriesCreatePort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventSeriesEditPort;
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

CalendarEventSaveRequest validSaveRequest()
{
    return CalendarEventSaveRequest{
        std::nullopt,
        "Typed save event",
        "2026-12-10",
        "2026-12-10",
        std::string("09:00"),
        std::string("10:00"),
        false,
        "Meeting",
        "Timed"
    };
}

CalendarEventSeriesCreateRequest seriesCreateRequest(
    const std::string& seriesId,
    const QList<QPair<QDate, QDate>>& dateRanges,
    const QString& title,
    const QString& eventType,
    const QString& timeStatus,
    const bool allDay = false
    )
{
    CalendarEventSeriesCreateRequest request;
    request.repeatSeriesId = seriesId;
    request.occurrences.reserve(static_cast<std::size_t>(dateRanges.size()));
    for (const QPair<QDate, QDate>& dateRange : dateRanges)
    {
        CalendarEventSaveRequest occurrence;
        occurrence.title = title.toUtf8().toStdString();
        occurrence.startDate = dateRange.first.toString(
            Qt::ISODate
            ).toStdString();
        occurrence.endDate = dateRange.second.toString(
            Qt::ISODate
            ).toStdString();
        occurrence.allDay = allDay;
        occurrence.eventType = eventType.toUtf8().toStdString();
        occurrence.timeStatus = timeStatus.toUtf8().toStdString();
        if (!allDay && timeStatus == QStringLiteral("Timed"))
        {
            occurrence.startTime = "09:15";
            occurrence.endTime = "10:45";
        }
        request.occurrences.push_back(std::move(occurrence));
    }

    return request;
}

CalendarEventSeriesEditRequest validSeriesEditRequest()
{
    return CalendarEventSeriesEditRequest{
        "series-edit",
        "2026-12-08",
        "2026-12-10",
        "2026-12-12",
        "Edited repeat event",
        std::string("13:15"),
        std::string("14:45"),
        false,
        "Workshop",
        "Timed"
    };
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
    void createsAndUpdatesValidEventWithTypedIdMapping();
    void reportsInvalidSaveRequestStructurally();
    void reportsUnavailableSaveServiceStructurally();
    void reportsSaveServiceFailureStructurally();
    void createsDailyWeeklyMonthlySeriesWithTypedIdsAndParity();
    void reportsInvalidSeriesCreateRequestStructurally();
    void reportsUnavailableSeriesCreateServiceStructurally();
    void reportsSeriesCreateBatchFailureWithoutPartialRows();
    void editsValidRepeatSeriesSuffixWithTypedParity();
    void propagatesAllDayAndUnknownTimePolicy();
    void reportsInvalidRepeatSeriesEditRequestStructurally();
    void reportsUnavailableRepeatSeriesEditServiceStructurally();
    void reportsRepeatSeriesEditReadFailureStructurally();
    void reportsRepeatSeriesEditSaveFailureStructurally();
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
createsAndUpdatesValidEventWithTypedIdMapping()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    ApplicationServicesCalendarEventSavePort port(services);
    const CalendarEventSaveRequest createRequest = validSaveRequest();
    const auto created = port.saveEvent(createRequest);
    QVERIFY(created);

    const auto createdId = created.value();
    QVERIFY(createdId.value().size() <= 10);
    const bool parsedCreatedId = createdId.value() != "0";
    QVERIFY(parsedCreatedId);

    bool converted = false;
    const int legacyId = QString::fromStdString(createdId.value()).toInt(
        &converted
        );
    QVERIFY(converted);
    QVERIFY(legacyId > 0);
    const auto loadedCreated = legacyService->event(legacyId);
    QVERIFY(loadedCreated);
    QCOMPARE(loadedCreated->id, legacyId);
    QCOMPARE(loadedCreated->title, QStringLiteral("Typed save event"));
    QCOMPARE(loadedCreated->eventType, QStringLiteral("Meeting"));
    QCOMPARE(loadedCreated->timeStatus, QStringLiteral("Timed"));
    QCOMPARE(loadedCreated->startDate, QDate(2026, 12, 10));
    QCOMPARE(loadedCreated->startTime, QTime(9, 0));
    QCOMPARE(loadedCreated->endTime, QTime(10, 0));
    QVERIFY(loadedCreated->repeatSeriesId.isEmpty());

    auto updateRequest = createRequest;
    updateRequest.id = createdId;
    updateRequest.title = "Typed updated event";
    updateRequest.startDate = "2026-12-11";
    updateRequest.endDate = "2026-12-12";
    updateRequest.startTime = "11:00";
    updateRequest.endTime = "12:30";
    const auto updated = port.saveEvent(updateRequest);
    QVERIFY(updated);
    QCOMPARE(updated.value(), createdId);

    const auto loadedUpdated = legacyService->event(legacyId);
    QVERIFY(loadedUpdated);
    QCOMPARE(loadedUpdated->id, legacyId);
    QCOMPARE(loadedUpdated->title, QStringLiteral("Typed updated event"));
    QCOMPARE(loadedUpdated->startDate, QDate(2026, 12, 11));
    QCOMPARE(loadedUpdated->endDate, QDate(2026, 12, 12));
    QCOMPARE(loadedUpdated->startTime, QTime(11, 0));
    QCOMPARE(loadedUpdated->endTime, QTime(12, 30));
    QCOMPARE(
        legacyService->eventsInRange(
            QDate(2026, 12, 10),
            QDate(2026, 12, 12)
            )->size(),
        1
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsInvalidSaveRequestStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesCalendarEventSavePort port(services);

    auto blankTitle = validSaveRequest();
    blankTitle.title = "   ";
    verifyFailure(port.saveEvent(blankTitle), ErrorCode::InvalidInput);

    auto invalidDate = validSaveRequest();
    invalidDate.startDate = "2026-02-30";
    verifyFailure(port.saveEvent(invalidDate), ErrorCode::InvalidInput);

    auto invalidStatus = validSaveRequest();
    invalidStatus.timeStatus = "Unknown";
    verifyFailure(port.saveEvent(invalidStatus), ErrorCode::InvalidInput);

    auto allDayWithTimes = validSaveRequest();
    allDayWithTimes.allDay = true;
    verifyFailure(
        port.saveEvent(allDayWithTimes),
        ErrorCode::InvalidInput
        );

    auto invalidUpdateId = validSaveRequest();
    invalidUpdateId.id = *CalendarEventId::fromString("0");
    verifyFailure(
        port.saveEvent(invalidUpdateId),
        ErrorCode::InvalidInput
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableSaveServiceStructurally()
{
    ApplicationServices services;
    ApplicationServicesCalendarEventSavePort port(services);

    verifyFailure(
        port.saveEvent(validSaveRequest()),
        ErrorCode::NotFound
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsSaveServiceFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_save "
        "BEFORE INSERT ON calendar_events "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar save failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventSavePort port(services);
    const auto saved = port.saveEvent(validSaveRequest());

    verifyFailure(saved, ErrorCode::Technical);
    QVERIFY(
        saved.error().message.find("Creating calendar event")
            != std::string::npos
        || saved.error().message.find("injected calendar save failure")
            != std::string::npos
    );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
createsDailyWeeklyMonthlySeriesWithTypedIdsAndParity()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    ApplicationServicesCalendarEventSeriesCreatePort port(services);
    const QList<CalendarEventSeriesCreateRequest> requests{
        seriesCreateRequest(
            "series-daily",
            {
                {QDate(2026, 12, 1), QDate(2026, 12, 2)},
                {QDate(2026, 12, 2), QDate(2026, 12, 3)},
                {QDate(2026, 12, 3), QDate(2026, 12, 4)}
            },
            QStringLiteral("Daily title"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed")
            ),
        seriesCreateRequest(
            "series-weekly",
            {
                {QDate(2026, 12, 1), QDate(2026, 12, 2)},
                {QDate(2026, 12, 8), QDate(2026, 12, 9)},
                {QDate(2026, 12, 15), QDate(2026, 12, 16)}
            },
            QStringLiteral("Weekly title"),
            QStringLiteral("Workshop"),
            QStringLiteral("Unknown")
            ),
        seriesCreateRequest(
            "series-monthly",
            {
                {QDate(2026, 1, 31), QDate(2026, 2, 2)},
                {QDate(2026, 2, 28), QDate(2026, 3, 2)},
                {QDate(2026, 3, 28), QDate(2026, 3, 30)}
            },
            QStringLiteral("Monthly title"),
            QStringLiteral("Vacation"),
            QStringLiteral("Timed"),
            true
            )
    };

    const QList<QString> seriesIds{
        QStringLiteral("series-daily"),
        QStringLiteral("series-weekly"),
        QStringLiteral("series-monthly")
    };
    const QList<QString> titles{
        QStringLiteral("Daily title"),
        QStringLiteral("Weekly title"),
        QStringLiteral("Monthly title")
    };
    const QList<QList<QPair<QDate, QDate>>> expectedRanges{
        {
            {QDate(2026, 12, 1), QDate(2026, 12, 2)},
            {QDate(2026, 12, 2), QDate(2026, 12, 3)},
            {QDate(2026, 12, 3), QDate(2026, 12, 4)}
        },
        {
            {QDate(2026, 12, 1), QDate(2026, 12, 2)},
            {QDate(2026, 12, 8), QDate(2026, 12, 9)},
            {QDate(2026, 12, 15), QDate(2026, 12, 16)}
        },
        {
            {QDate(2026, 1, 31), QDate(2026, 2, 2)},
            {QDate(2026, 2, 28), QDate(2026, 3, 2)},
            {QDate(2026, 3, 28), QDate(2026, 3, 30)}
        }
    };

    for (int requestIndex = 0; requestIndex < requests.size();
         ++requestIndex)
    {
        const auto created = port.createRepeatSeries(
            requests.at(requestIndex)
            );
        QVERIFY(created);
        QCOMPARE(created.value().size(), std::size_t(3));

        const std::vector<CalendarEventSaveRequest>& occurrences =
            requests.at(requestIndex).occurrences;
        for (std::size_t occurrenceIndex = 0;
             occurrenceIndex < created.value().size();
             ++occurrenceIndex)
        {
            const CalendarEventId& typedId = created.value().at(occurrenceIndex);
            bool converted = false;
            const int legacyId = QString::fromStdString(typedId.value()).toInt(
                &converted
                );
            QVERIFY(converted);
            QVERIFY(legacyId > 0);
            QVERIFY(legacyService->event(legacyId));

            const auto loaded = legacyService->event(legacyId);
            QVERIFY(loaded);
            QCOMPARE(
                loaded->repeatSeriesId,
                seriesIds.at(requestIndex)
                );
            QCOMPARE(loaded->title, titles.at(requestIndex));
            QCOMPARE(
                loaded->startDate,
                expectedRanges.at(requestIndex).at(
                    static_cast<int>(occurrenceIndex)
                    ).first
                );
            QCOMPARE(
                loaded->endDate,
                expectedRanges.at(requestIndex).at(
                    static_cast<int>(occurrenceIndex)
                    ).second
                );
            QCOMPARE(
                loaded->eventType,
                QString::fromStdString(
                    occurrences.at(occurrenceIndex).eventType
                    )
                );
            QCOMPARE(
                loaded->timeStatus,
                QString::fromStdString(
                    occurrences.at(occurrenceIndex).timeStatus
                    )
                );

            if (requestIndex == 0)
            {
                QCOMPARE(loaded->startTime, QTime(9, 15));
                QCOMPARE(loaded->endTime, QTime(10, 45));
                QVERIFY(!loaded->allDay);
            }
            else if (requestIndex == 1)
            {
                QVERIFY(!loaded->allDay);
                QVERIFY(!loaded->startTime.isValid());
                QVERIFY(!loaded->endTime.isValid());
            }
            else
            {
                QVERIFY(loaded->allDay);
                QVERIFY(!loaded->startTime.isValid());
                QVERIFY(!loaded->endTime.isValid());
            }
        }
    }
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsInvalidSeriesCreateRequestStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesCalendarEventSeriesCreatePort port(services);

    auto blankSeries = seriesCreateRequest(
        "series-invalid",
        {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
        QStringLiteral("Valid title"),
        QStringLiteral("Meeting"),
        QStringLiteral("Timed")
        );
    blankSeries.repeatSeriesId = " \t";
    verifyFailure(
        port.createRepeatSeries(blankSeries),
        ErrorCode::InvalidInput
        );

    auto oversizedSeries = seriesCreateRequest(
        std::string(
            kCalendarEventSeriesCreateMaxRepeatSeriesIdLength + 1,
            'r'
            ),
        {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
        QStringLiteral("Valid title"),
        QStringLiteral("Meeting"),
        QStringLiteral("Timed")
        );
    verifyFailure(
        port.createRepeatSeries(oversizedSeries),
        ErrorCode::InvalidInput
        );

    auto emptySeries = blankSeries;
    emptySeries.repeatSeriesId = "series-empty";
    emptySeries.occurrences.clear();
    verifyFailure(
        port.createRepeatSeries(emptySeries),
        ErrorCode::InvalidInput
        );

    auto invalidOccurrence = seriesCreateRequest(
        "series-invalid-occurrence",
        {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
        QStringLiteral("Valid title"),
        QStringLiteral("Meeting"),
        QStringLiteral("Timed")
        );
    invalidOccurrence.occurrences.front().endTime = "08:00";
    verifyFailure(
        port.createRepeatSeries(invalidOccurrence),
        ErrorCode::InvalidInput
        );

    auto tooManyOccurrences = seriesCreateRequest(
        "series-too-many",
        {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
        QStringLiteral("Valid title"),
        QStringLiteral("Meeting"),
        QStringLiteral("Timed")
        );
    const CalendarEventSaveRequest occurrence =
        tooManyOccurrences.occurrences.front();
    tooManyOccurrences.occurrences.assign(
        kCalendarEventSeriesCreateMaxOccurrences + 1,
        occurrence
        );
    verifyFailure(
        port.createRepeatSeries(tooManyOccurrences),
        ErrorCode::InvalidInput
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableSeriesCreateServiceStructurally()
{
    ApplicationServices services;
    ApplicationServicesCalendarEventSeriesCreatePort port(services);

    verifyFailure(
        port.createRepeatSeries(seriesCreateRequest(
            "series-unavailable",
            {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
            QStringLiteral("Valid title"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed")
            )),
        ErrorCode::NotFound
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsSeriesCreateBatchFailureWithoutPartialRows()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_series_second_occurrence "
        "BEFORE INSERT ON calendar_events "
        "WHEN NEW.start_date = '2026-12-02' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar series save failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventSeriesCreatePort port(services);
    const auto saved = port.createRepeatSeries(seriesCreateRequest(
        "series-atomic-failure",
        {
            {QDate(2026, 12, 1), QDate(2026, 12, 1)},
            {QDate(2026, 12, 2), QDate(2026, 12, 2)},
            {QDate(2026, 12, 3), QDate(2026, 12, 3)}
        },
        QStringLiteral("Atomic series"),
        QStringLiteral("Meeting"),
        QStringLiteral("Timed")
        ));

    verifyFailure(saved, ErrorCode::Technical);
    QVERIFY(
        saved.error().message.find("injected calendar series save failure")
            != std::string::npos
        );

    const auto loaded = services.calendarService()->eventsInRange(
        QDate(2026, 12, 1),
        QDate(2026, 12, 3)
        );
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
editsValidRepeatSeriesSuffixWithTypedParity()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    const QString repeatSeriesId = QStringLiteral("series-edit");
    const QList<QDate> occurrenceDates = {
        QDate(2026, 12, 1),
        QDate(2026, 12, 8),
        QDate(2026, 12, 15),
        QDate(2026, 12, 22)
    };
    QList<int> eventIds;
    for (qsizetype index = 0; index < occurrenceDates.size(); ++index)
    {
        const QDate startDate = occurrenceDates.at(index);
        CalendarEvent event = makeEvent(
            QStringLiteral("Original %1").arg(index + 1),
            startDate,
            startDate.addDays(1)
            );
        event.startTime = QTime(9, 0);
        event.endTime = QTime(10, 0);
        event.eventType = QStringLiteral("Meeting");
        event.timeStatus = QStringLiteral("Timed");
        event.repeatSeriesId = repeatSeriesId;
        const int eventId = saveEvent(*legacyService, event);
        QVERIFY(eventId > 0);
        eventIds.append(eventId);
    }

    const auto before = legacyService->repeatSeriesFromDate(
        repeatSeriesId,
        QDate(2026, 12, 1)
        );
    QVERIFY(before);
    QCOMPARE(before->size(), occurrenceDates.size());
    for (qsizetype index = 0; index < before->size(); ++index)
    {
        QCOMPARE(before->at(index).id, eventIds.at(index));
    }

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    auto request = validSeriesEditRequest();
    request.repeatSeriesId = " series-edit ";
    const auto edited = port.editRepeatSeriesFromDate(request);
    QVERIFY(edited);

    const auto after = legacyService->repeatSeriesFromDate(
        repeatSeriesId,
        QDate(2026, 12, 1)
        );
    QVERIFY(after);
    QCOMPARE(after->size(), occurrenceDates.size());
    for (qsizetype index = 0; index < after->size(); ++index)
    {
        QCOMPARE(after->at(index).id, eventIds.at(index));
    }

    const CalendarEvent& untouched = after->at(0);
    QCOMPARE(untouched.title, QStringLiteral("Original 1"));
    QCOMPARE(untouched.startDate, QDate(2026, 12, 1));
    QCOMPARE(untouched.endDate, QDate(2026, 12, 2));
    QCOMPARE(untouched.startTime, QTime(9, 0));
    QCOMPARE(untouched.endTime, QTime(10, 0));

    for (qsizetype index = 1; index < after->size(); ++index)
    {
        const CalendarEvent& event = after->at(index);
        QCOMPARE(event.id, eventIds.at(index));
        QCOMPARE(event.title, QStringLiteral("Edited repeat event"));
        QCOMPARE(event.eventType, QStringLiteral("Workshop"));
        QCOMPARE(event.timeStatus, QStringLiteral("Timed"));
        QCOMPARE(event.repeatSeriesId, repeatSeriesId);
        QCOMPARE(
            event.startDate,
            occurrenceDates.at(index).addDays(2)
            );
        QCOMPARE(event.endDate, event.startDate.addDays(2));
        QCOMPARE(event.startTime, QTime(13, 15));
        QCOMPARE(event.endTime, QTime(14, 45));
    }
}

void NextPlatformApplicationServicesCalendarEventPortTests::
propagatesAllDayAndUnknownTimePolicy()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    const QString repeatSeriesId = QStringLiteral("series-time-policy");
    const QList<QDate> occurrenceDates = {
        QDate(2027, 1, 5),
        QDate(2027, 1, 12),
        QDate(2027, 1, 19)
    };
    QList<int> eventIds;
    for (const QDate& startDate : occurrenceDates)
    {
        CalendarEvent event = makeEvent(
            QStringLiteral("Policy event"),
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

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    CalendarEventSeriesEditRequest allDayRequest{
        repeatSeriesId.toUtf8().toStdString(),
        "2027-01-05",
        "2027-01-06",
        "2027-01-08",
        "All-day policy",
        std::nullopt,
        std::nullopt,
        true,
        "Holiday",
        "Timed"
    };
    QVERIFY(port.editRepeatSeriesFromDate(allDayRequest));

    for (qsizetype index = 0; index < eventIds.size(); ++index)
    {
        const auto loaded = legacyService->event(eventIds.at(index));
        QVERIFY(loaded);
        QCOMPARE(loaded->id, eventIds.at(index));
        QCOMPARE(loaded->title, QStringLiteral("All-day policy"));
        QCOMPARE(loaded->eventType, QStringLiteral("Holiday"));
        QCOMPARE(loaded->timeStatus, QStringLiteral("Timed"));
        QVERIFY(loaded->allDay);
        QVERIFY(!loaded->startTime.isValid());
        QVERIFY(!loaded->endTime.isValid());
        QCOMPARE(loaded->startDate, occurrenceDates.at(index).addDays(1));
        QCOMPARE(loaded->endDate, loaded->startDate.addDays(2));
    }

    CalendarEventSeriesEditRequest unknownRequest{
        repeatSeriesId.toUtf8().toStdString(),
        "2027-01-12",
        "2027-01-14",
        "2027-01-15",
        "Unknown policy",
        std::nullopt,
        std::nullopt,
        false,
        "Vacation",
        "Unknown"
    };
    QVERIFY(port.editRepeatSeriesFromDate(unknownRequest));

    const auto untouched = legacyService->event(eventIds.at(0));
    QVERIFY(untouched);
    QVERIFY(untouched->allDay);
    QCOMPARE(untouched->title, QStringLiteral("All-day policy"));

    for (qsizetype index = 1; index < eventIds.size(); ++index)
    {
        const auto loaded = legacyService->event(eventIds.at(index));
        QVERIFY(loaded);
        QCOMPARE(loaded->id, eventIds.at(index));
        QCOMPARE(loaded->title, QStringLiteral("Unknown policy"));
        QCOMPARE(loaded->eventType, QStringLiteral("Vacation"));
        QCOMPARE(loaded->timeStatus, QStringLiteral("Unknown"));
        QVERIFY(!loaded->allDay);
        QVERIFY(!loaded->startTime.isValid());
        QVERIFY(!loaded->endTime.isValid());
    }
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsInvalidRepeatSeriesEditRequestStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    auto invalid = validSeriesEditRequest();
    invalid.repeatSeriesId = " \t";
    verifyFailure(
        port.editRepeatSeriesFromDate(invalid),
        ErrorCode::InvalidInput
        );

    invalid = validSeriesEditRequest();
    invalid.startDate = "2026-02-30";
    verifyFailure(
        port.editRepeatSeriesFromDate(invalid),
        ErrorCode::InvalidInput
        );

    invalid = validSeriesEditRequest();
    invalid.editedEndDate = "2026-12-09";
    verifyFailure(
        port.editRepeatSeriesFromDate(invalid),
        ErrorCode::InvalidInput
        );

    invalid = validSeriesEditRequest();
    invalid.allDay = true;
    verifyFailure(
        port.editRepeatSeriesFromDate(invalid),
        ErrorCode::InvalidInput
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableRepeatSeriesEditServiceStructurally()
{
    ApplicationServices services;
    ApplicationServicesCalendarEventSeriesEditPort port(services);

    verifyFailure(
        port.editRepeatSeriesFromDate(validSeriesEditRequest()),
        ErrorCode::NotFound
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsRepeatSeriesEditReadFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY(query.exec(QStringLiteral("DROP TABLE calendar_events")));

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    const auto edited = port.editRepeatSeriesFromDate(
        validSeriesEditRequest()
        );

    verifyFailure(edited, ErrorCode::Technical);
    QVERIFY(
        edited.error().message.find("calendar") != std::string::npos
        || edited.error().message.find("Calendar") != std::string::npos
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsRepeatSeriesEditSaveFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    const QString repeatSeriesId = QStringLiteral("series-edit-failure");
    QList<int> eventIds;
    for (const QDate& startDate : {
             QDate(2026, 12, 8),
             QDate(2026, 12, 15)
         })
    {
        CalendarEvent event = makeEvent(
            QStringLiteral("Persisted before failure"),
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

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_repeat_edit "
        "BEFORE UPDATE ON calendar_events "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar repeat edit failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    auto request = validSeriesEditRequest();
    request.repeatSeriesId = repeatSeriesId.toUtf8().toStdString();
    const auto edited = port.editRepeatSeriesFromDate(request);

    verifyFailure(edited, ErrorCode::Technical);
    QVERIFY(
        edited.error().message.find("Updating calendar event")
            != std::string::npos
        || edited.error().message.find("injected calendar repeat edit")
            != std::string::npos
        );

    for (const int eventId : eventIds)
    {
        const auto loaded = legacyService->event(eventId);
        QVERIFY(loaded);
        QCOMPARE(loaded->title, QStringLiteral("Persisted before failure"));
        QCOMPARE(loaded->repeatSeriesId, repeatSeriesId);
        QCOMPARE(loaded->startTime, QTime(9, 0));
        QCOMPARE(loaded->endTime, QTime(10, 0));
    }
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
    using SavePort = CalendarEventSavePort;
    using SeriesEditPort = CalendarEventSeriesEditPort;
    using SeriesDeletePort = CalendarEventSeriesDeletePort;
    using SeriesCreatePort = CalendarEventSeriesCreatePort;
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
    using SaveResult = decltype(
        std::declval<SavePort&>().saveEvent(
            std::declval<const CalendarEventSaveRequest&>()
            )
        );
    using SeriesDeleteResult = decltype(
        std::declval<SeriesDeletePort&>().deleteRepeatSeriesFromDate(
            std::declval<const CalendarEventSeriesDeleteRequest&>()
            )
        );
    using SeriesEditResult = decltype(
        std::declval<SeriesEditPort&>().editRepeatSeriesFromDate(
            std::declval<const CalendarEventSeriesEditRequest&>()
            )
        );
    using SeriesCreateResult = decltype(
        std::declval<SeriesCreatePort&>().createRepeatSeries(
            std::declval<const CalendarEventSeriesCreateRequest&>()
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
        SaveResult,
        Domain::Result<CalendarEventId>
        >);
    static_assert(std::is_same_v<
        SeriesDeleteResult,
        Domain::Result<void>
        >);
    static_assert(std::is_same_v<
        SeriesEditResult,
        Domain::Result<void>
        >);
    static_assert(std::is_same_v<
        SeriesCreateResult,
        Domain::Result<std::vector<CalendarEventId>>
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
        SavePort,
        ApplicationServicesCalendarEventSavePort
        >);
    static_assert(!std::is_copy_constructible_v<
        ApplicationServicesCalendarEventSavePort
        >);
    static_assert(!std::is_move_constructible_v<
        ApplicationServicesCalendarEventSavePort
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSaveRequest>().id),
        std::optional<CalendarEventId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSaveRequest>().allDay),
        bool
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
    static_assert(std::is_base_of_v<
        SeriesEditPort,
        ApplicationServicesCalendarEventSeriesEditPort
        >);
    static_assert(!std::is_copy_constructible_v<
        ApplicationServicesCalendarEventSeriesEditPort
        >);
    static_assert(!std::is_move_constructible_v<
        ApplicationServicesCalendarEventSeriesEditPort
        >);
    static_assert(std::is_base_of_v<
        SeriesCreatePort,
        ApplicationServicesCalendarEventSeriesCreatePort
        >);
    static_assert(!std::is_copy_constructible_v<
        ApplicationServicesCalendarEventSeriesCreatePort
        >);
    static_assert(!std::is_move_constructible_v<
        ApplicationServicesCalendarEventSeriesCreatePort
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesCreateRequest>()
                     .repeatSeriesId),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesCreateRequest>()
                     .occurrences),
        std::vector<CalendarEventSaveRequest>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().repeatSeriesId),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().startDate),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().editedStartDate),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().editedEndDate),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().startTime),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().endTime),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().allDay),
        bool
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

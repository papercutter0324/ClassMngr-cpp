#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "next/application/calendar_event_by_id_query_port.h"
#include "next/application/calendar_event_delete_port.h"
#include "next/application/calendar_event_delete_all_port.h"
#include "next/application/calendar_event_import_save_port.h"
#include "next/application/calendar_event_import_signature_query_port.h"
#include "next/application/calendar_event_save_port.h"
#include "next/application/calendar_event_save_use_case.h"
#include "next/application/calendar_event_series_create_port.h"
#include "next/application/calendar_event_series_edit_port.h"
#include "next/application/calendar_event_series_delete_port.h"
#include "next/domain/gregorian_date.h"
#include "next/application/sub_prep_calendar_event_intervals_query.h"
#include "next/platform/application_services_calendar_event_delete_port.h"
#include "next/platform/application_services_calendar_event_delete_all_port.h"
#include "next/platform/application_services_calendar_event_import_save_port.h"
#include "next/platform/application_services_calendar_event_import_signature_query_port.h"
#include "next/platform/application_services_calendar_event_save_port.h"
#include "next/platform/application_services_calendar_event_series_create_port.h"
#include "next/platform/application_services_calendar_event_series_edit_port.h"
#include "next/platform/application_services_calendar_event_series_delete_port.h"
#include "next/platform/application_services_calendar_event_port.h"
#include "next/platform/calendar_event_time_status_adapter.h"
#include "next/platform/application_services_sub_prep_calendar_event_intervals_port.h"
#include "features/calendar/academic_calendar_event_parser.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using ClassMngr::Next::Platform::ApplicationServicesCalendarEventDeletePort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventDeleteAllPort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventImportSavePort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventImportSignatureQueryPort;
using ClassMngr::Next::Platform::ApplicationServicesCalendarEventSavePort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventSeriesCreatePort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventSeriesEditPort;
using ClassMngr::Next::Platform::
    ApplicationServicesCalendarEventSeriesDeletePort;
using ClassMngr::Next::Platform::ApplicationServicesCalendarEventPort;
using ClassMngr::Next::Platform::
    ApplicationServicesSubPrepCalendarEventIntervalsPort;

namespace
{

GregorianDate gregorianDate(const std::string_view isoDate)
{
    return *GregorianDate::fromIsoString(isoDate);
}

std::string repeatedHangulTitle(const std::size_t count)
{
    QString title;
    for (std::size_t index = 0; index < count; ++index)
    {
        title.append(QChar(0xAC00));
    }
    return title.toUtf8().toStdString();
}

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

CalendarEventId calendarEventId(const qint64 id)
{
    return *CalendarEventId::fromString(std::to_string(id));
}

CalendarEventSeriesId calendarEventSeriesId(const std::string& id)
{
    return *CalendarEventSeriesId::fromString(id);
}

CalendarEventSaveRequest validSaveRequest()
{
    return CalendarEventSaveRequest{
        std::nullopt,
        "Typed save event",
        gregorianDate("2026-12-10"),
        gregorianDate("2026-12-10"),
        std::string("09:00"),
        std::string("10:00"),
        false,
        "Meeting",
        CalendarEventTimeStatus::Timed
    };
}

CalendarEventSaveInput saveInputFromRequest(
    const CalendarEventSaveRequest& request
    )
{
    return {
        request.id,
        request.title,
        request.startDate.toIsoString(),
        request.endDate.toIsoString(),
        request.startTime,
        request.endTime,
        request.allDay,
        request.eventType,
        request.timeStatus
    };
}

CalendarEventImportSignatureRangeRequest importSignatureRange(
    const QDate& startDate,
    const QDate& endDate
    )
{
    return {
        CalendarEventDate(startDate.toString(Qt::ISODate).toStdString()),
        CalendarEventDate(endDate.toString(Qt::ISODate).toStdString())
    };
}

CalendarEventImportSaveRequest validImportSaveRequest()
{
    CalendarEventSaveRequest laterEvent = validSaveRequest();
    laterEvent.title = "Later imported event";
    laterEvent.startDate = gregorianDate("2026-12-12");
    laterEvent.endDate = gregorianDate("2026-12-12");
    laterEvent.startTime.reset();
    laterEvent.endTime.reset();
    laterEvent.eventType = "Workshop";
    laterEvent.timeStatus = CalendarEventTimeStatus::Unknown;

    CalendarEventSaveRequest earlierAllDayEvent = validSaveRequest();
    earlierAllDayEvent.title = "Earlier imported event";
    earlierAllDayEvent.startDate = gregorianDate("2026-12-10");
    earlierAllDayEvent.endDate = gregorianDate("2026-12-10");
    earlierAllDayEvent.startTime.reset();
    earlierAllDayEvent.endTime.reset();
    earlierAllDayEvent.allDay = true;
    earlierAllDayEvent.eventType = "Holiday";

    CalendarEventImportSaveRequest request;
    request.events = {
        std::move(laterEvent),
        std::move(earlierAllDayEvent)
    };
    return request;
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
    request.repeatSeriesId = calendarEventSeriesId(seriesId);
    request.occurrences.reserve(static_cast<std::size_t>(dateRanges.size()));
    for (const QPair<QDate, QDate>& dateRange : dateRanges)
    {
        CalendarEventSaveRequest occurrence = validSaveRequest();
        occurrence.title = title.toUtf8().toStdString();
        occurrence.startDate = gregorianDate(dateRange.first.toString(
            Qt::ISODate
            ).toStdString());
        occurrence.endDate = gregorianDate(dateRange.second.toString(
            Qt::ISODate
            ).toStdString());
        occurrence.allDay = allDay;
        occurrence.eventType = eventType.toUtf8().toStdString();
        occurrence.timeStatus =
            ClassMngr::Next::Platform::calendarEventTimeStatusFromLegacyText(
                timeStatus
                );
        occurrence.startTime.reset();
        occurrence.endTime.reset();
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
        calendarEventSeriesId("series-edit"),
        *GregorianDate::fromIsoString("2026-12-08"),
        *GregorianDate::fromIsoString("2026-12-10"),
        *GregorianDate::fromIsoString("2026-12-12"),
        "Edited repeat event",
        std::string("13:15"),
        std::string("14:45"),
        false,
        CalendarEventType::Workshop,
        CalendarEventTimeStatus::Timed
    };
}

enum class CalendarEventWriteOperation
{
    Save,
    ImportSave,
    SeriesCreate,
    SeriesEdit
};

void setInvalidTime(
    CalendarEventSaveRequest& request,
    const QString& value,
    const bool startTime
    )
{
    if (startTime)
    {
        request.startTime = value.toStdString();
    }
    else
    {
        request.endTime = value.toStdString();
    }
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

template <typename Value>
void verifyRecoverableFailure(
    const Domain::Result<Value>& result,
    const ErrorCode expectedCode,
    const std::string& expectedMessage
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, expectedCode);
    QCOMPARE(result.error().message, expectedMessage);
    QVERIFY(result.error().recoverable);
}

template <typename Value>
void verifyPublicTimeBoundaryFailure(
    const Domain::Result<void>& requestValidation,
    const Domain::Result<Value>& portResult,
    const std::string& expectedMessage
    )
{
    verifyFailure(requestValidation, ErrorCode::InvalidInput);
    QCOMPARE(requestValidation.error().message, expectedMessage);
    verifyFailure(portResult, ErrorCode::InvalidInput);
    QCOMPARE(portResult.error().message, expectedMessage);
}

} // namespace

class NextPlatformApplicationServicesCalendarEventPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void statusAdapterPreservesLegacyNormalizationAndCanonicalNames();
    void initTestCase();
    void widePersistedIdentity_data();
    void widePersistedIdentity();
    void generatedWideIdentities_data();
    void generatedWideIdentities();
    void reportsCalendarEventPortAvailability();
    void projectsOverlappingRangeAsOwnedTypedMetadata();
    void readsTwoCalendarYearsOfSubPrepIntervalsWithoutProjectionTruncation();
    void readsTwoYearSubPrepIntervalsThroughApplicationServices();
    void reportsUnavailableSubPrepIntervalSessionsStructurally();
    void reportsSubPrepIntervalRepositoryFailureStructurally();
    void projectsByIdAsOwnedTypedMetadata();
    void deletesValidTypedEvent();
    void deletesMissingValidTypedEventSuccessfully();
    void reportsInvalidDeleteIdStructurally();
    void reportsUnavailableDeleteSessionStructurally();
    void reportsDeleteServiceFailureStructurally();
    void deletesAllCalendarEventsThroughTypedPort();
    void reportsUnavailableDeleteAllSessionStructurally();
    void reportsDeleteAllServiceFailureStructurally();
    void savesOrderedCalendarImportBatchWithTypedIdsAndParity();
    void acceptsDuplicateOnlyEmptyImportBatchAsNoOp();
    void rejectsInvalidImportBatchBeforeCreatingAnyRows();
    void reportsImportBatchFailureWithoutPartialRows();
    void reportsUnavailableImportSaveSessionStructurally();
    void titleAdaptersUseUtf16UnitBoundariesForSqlReadAndWrite();
    void createsAndUpdatesValidEventWithTypedIdMapping();
    void reportsMissingCalendarEventUpdateStructurally();
    void reportsCalendarEventUpdateSqlFailureStructurally();
    void reportsInvalidSaveRequestStructurally();
    void rejectsPaddedAndMalformedTypedTimesAtPublicBoundary_data();
    void rejectsPaddedAndMalformedTypedTimesAtPublicBoundary();
    void reportsUnavailableSaveSessionStructurally();
    void reportsSaveServiceFailureStructurally();
    void createsDailyWeeklyMonthlySeriesWithTypedIdsAndParity();
    void reportsInvalidSeriesCreateRequestStructurally();
    void rejectsExistingOccurrenceIdBeforePersistence();
    void reportsUnavailableSeriesCreateSessionStructurally();
    void reportsSeriesCreateBatchFailureWithoutPartialRows();
    void editsValidRepeatSeriesSuffixWithTypedParity();
    void acceptsEmptyRepeatSeriesEditSuffixAsNoOp();
    void reportsInvalidRepeatSeriesSourceDateBeforePersistence();
    void reportsRepeatSeriesDateOverflowBeforePersistence();
    void propagatesAllDayAndUnknownTimePolicy();
    void reportsInvalidRepeatSeriesEditRequestStructurally();
    void reportsUnavailableRepeatSeriesEditSessionStructurally();
    void reportsRepeatSeriesEditReadFailureStructurally();
    void reportsRepeatSeriesEditTerminalReadFailureBeforePersistence();
    void reportsRepeatSeriesEditSaveFailureStructurally();
    void deletesValidRepeatSeriesSuffix();
    void deletesMissingRepeatSeriesSuffixSuccessfully();
    void reportsInvalidRepeatSeriesDeleteRequestStructurally();
    void reportsUnavailableRepeatSeriesDeleteSessionStructurally();
    void reportsRepeatSeriesDeleteServiceFailureStructurally();
    void preservesLegacyTitleSurroundingSpaces();
    void preservesAllDayAndUnknownTimePolicy();
    void reportsUnavailableAndInvalidRangesStructurally();
    void reportsCalendarEventRepositoryReadFailuresStructurally();
    void returnsLegacyImportSignatureKeysInRangeOrder();
    void reportsUnavailableAndInvalidImportSignatureRangesStructurally();
    void reportsImportSignatureRangeReadFailureStructurally();
    void rejectsPartialSourceTimesAndProjectionOverflow();
    void rejectsMalformedRepeatSeriesMetadataStructurally();
    void boundaryIsTypedAndDoesNotExposeLegacyOwnership();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesCalendarEventPortTests::
statusAdapterPreservesLegacyNormalizationAndCanonicalNames()
{
    using ClassMngr::Next::Platform::calendarEventTimeStatusFromLegacyText;
    using ClassMngr::Next::Platform::calendarEventTimeStatusToLegacyText;

    QCOMPARE(
        calendarEventTimeStatusFromLegacyText(QStringLiteral(" Timed \t")),
        CalendarEventTimeStatus::Timed
        );
    QCOMPARE(
        calendarEventTimeStatusFromLegacyText(QStringLiteral("\nUnknown  ")),
        CalendarEventTimeStatus::Unknown
        );
    QCOMPARE(
        calendarEventTimeStatusFromLegacyText(
            QStringLiteral(" \tUnconfirmed\n")
            ),
        CalendarEventTimeStatus::Unconfirmed
        );
    QCOMPARE(
        calendarEventTimeStatusFromLegacyText(QStringLiteral("unknown")),
        CalendarEventTimeStatus::Timed
        );
    QCOMPARE(
        calendarEventTimeStatusFromLegacyText(
            QStringLiteral("not-a-time-status")
            ),
        CalendarEventTimeStatus::Timed
        );

    QCOMPARE(
        calendarEventTimeStatusToLegacyText(CalendarEventTimeStatus::Timed),
        QStringLiteral("Timed")
        );
    QCOMPARE(
        calendarEventTimeStatusToLegacyText(CalendarEventTimeStatus::Unknown),
        QStringLiteral("Unknown")
        );
    QCOMPARE(
        calendarEventTimeStatusToLegacyText(
            CalendarEventTimeStatus::Unconfirmed
            ),
        QStringLiteral("Unconfirmed")
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::widePersistedIdentity_data()
{
    QTest::addColumn<qint64>("wideId");
    QTest::newRow("above-int-max") << qint64(2147483648LL);
    QTest::newRow("above-javascript-exact-range") << qint64(9007199254740993LL);
    QTest::newRow("signed-64-max") << qint64(9223372036854775807LL);
}

void NextPlatformApplicationServicesCalendarEventPortTests::widePersistedIdentity()
{
    QFETCH(qint64, wideId);
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    auto* repository = services.databaseSession()->calendarEventRepository();
    auto lowEvent = validSaveRequest();
    ApplicationServicesCalendarEventSavePort savePort(services);
    const auto low = savePort.saveEvent(lowEvent);
    QVERIFY(low);
    QCOMPARE(low.value().value(), std::string("1"));
    auto wideEvent = lowEvent;
    wideEvent.title = "Wide event";
    const auto created = savePort.saveEvent(wideEvent);
    QVERIFY(created);
    QSqlQuery query(services.databaseSession()->database());
    query.prepare("UPDATE calendar_events SET id=? WHERE id=2");
    query.addBindValue(wideId);
    QVERIFY(query.exec());

    const auto loaded = repository->getCalendarEvent(wideId);
    QVERIFY(loaded);
    QCOMPARE(loaded->id, wideId);
    QCOMPARE(loaded->title, QStringLiteral("Wide event"));
    QCOMPARE(services.dataService()->getCalendarEvent(wideId)->id, wideId);
    QCOMPARE(services.calendarService()->event(wideId)->id, wideId);
    ApplicationServicesCalendarEventPort readPort(services);
    const auto typed = readPort.loadEventById(calendarEventId(wideId));
    QVERIFY(typed);
    QCOMPARE(typed.value().id.value(), std::to_string(wideId));
    QCOMPARE(typed.value().title, std::string("Wide event"));

    CalendarEvent legacyUpdate = *loaded;
    legacyUpdate.title = QStringLiteral("Legacy updated wide event");
    const auto legacySaved = repository->saveCalendarEvent(legacyUpdate);
    QVERIFY(legacySaved);
    QCOMPARE(*legacySaved, wideId);
    QCOMPARE(repository->getCalendarEvent(1)->title, QString::fromStdString(lowEvent.title));
    QCOMPARE(repository->getCalendarEvent(wideId)->title, legacyUpdate.title);

    wideEvent.id = calendarEventId(wideId);
    wideEvent.title = "Updated wide event";
    const auto updated = savePort.saveEvent(wideEvent);
    QVERIFY(updated);
    QCOMPARE(updated.value().value(), std::to_string(wideId));
    QCOMPARE(repository->getCalendarEvent(wideId)->title, QStringLiteral("Updated wide event"));
    QCOMPARE(repository->getCalendarEvent(1)->title, QString::fromStdString(lowEvent.title));

    // A failed wide update must retain both identities and contents.
    QVERIFY(query.exec("CREATE TRIGGER reject_wide_update BEFORE UPDATE ON calendar_events "
                       "BEGIN SELECT RAISE(ABORT, 'wide update failure'); END"));
    wideEvent.title = "Rejected wide update";
    const auto rejected = savePort.saveEvent(wideEvent);
    verifyFailure(rejected, ErrorCode::Technical);
    QCOMPARE(repository->getCalendarEvent(wideId)->title, QStringLiteral("Updated wide event"));
    QVERIFY(query.exec("DROP TRIGGER reject_wide_update"));

    ApplicationServicesCalendarEventDeletePort deletePort(services);
    QVERIFY(deletePort.deleteEvent(calendarEventId(wideId)));
    QVERIFY(!repository->getCalendarEvent(wideId));
    QCOMPARE(repository->getCalendarEvent(1)->title, QString::fromStdString(lowEvent.title));
    // Exercise the repository's direct delete boundary with the same wide key.
    query.prepare("INSERT INTO calendar_events "
                  "(id,title,event_type,time_status,repeat_series_id,all_day,"
                  "start_date,start_time,end_date,end_time) "
                  "SELECT ?,title,event_type,time_status,repeat_series_id,all_day,"
                  "start_date,start_time,end_date,end_time FROM calendar_events WHERE id=1");
    query.addBindValue(wideId);
    QVERIFY(query.exec());
    QVERIFY(repository->deleteCalendarEvent(wideId));
    QVERIFY(!repository->getCalendarEvent(wideId));
    QCOMPARE(repository->getCalendarEvent(1)->title, QString::fromStdString(lowEvent.title));
    // The first unrepresentable signed-64 identity is rejected, never wrapped.
    const auto outOfRange = *CalendarEventId::fromString("9223372036854775808");
    verifyFailure(readPort.loadEventById(outOfRange), ErrorCode::InvalidInput);
    verifyFailure(deletePort.deleteEvent(outOfRange), ErrorCode::InvalidInput);
    wideEvent.id = outOfRange;
    verifyFailure(savePort.saveEvent(wideEvent), ErrorCode::InvalidInput);
    const auto nonCanonicalId = *CalendarEventId::fromString("0001");
    verifyFailure(
        readPort.loadEventById(nonCanonicalId),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        deletePort.deleteEvent(nonCanonicalId),
        ErrorCode::InvalidInput
        );
    wideEvent.id = nonCanonicalId;
    verifyFailure(savePort.saveEvent(wideEvent), ErrorCode::InvalidInput);
    QVERIFY(repository->getCalendarEvent(1));
}

void NextPlatformApplicationServicesCalendarEventPortTests::generatedWideIdentities_data()
{
    QTest::addColumn<qint64>("sequence");
    QTest::newRow("cross-int-max") << qint64(2147483647LL);
    QTest::newRow("cross-javascript-exact-range") << qint64(9007199254740992LL);
}

void NextPlatformApplicationServicesCalendarEventPortTests::generatedWideIdentities()
{
    QFETCH(qint64, sequence);
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    auto* repository = services.databaseSession()->calendarEventRepository();
    CalendarEvent event = makeEvent(QStringLiteral("Low"), QDate(2026,12,10), QDate(2026,12,10));
    event.allDay = true;
    const auto low = repository->saveCalendarEvent(event);
    QVERIFY(low);
    QCOMPARE(*low, qint64(1));
    QSqlQuery query(services.databaseSession()->database());
    query.prepare("UPDATE sqlite_sequence SET seq=? WHERE name='calendar_events'");
    query.addBindValue(sequence);
    QVERIFY(query.exec());
    const auto single = repository->saveCalendarEvent(event);
    QVERIFY(single);
    QCOMPARE(*single, sequence + 1);
    const auto batch = repository->saveCalendarEvents({event, event});
    QVERIFY(batch);
    QCOMPARE(*batch, QList<qint64>({sequence + 2, sequence + 3}));
    ApplicationServicesCalendarEventSavePort savePort(services);
    const auto typedSingle = savePort.saveEvent(validSaveRequest());
    QVERIFY(typedSingle);
    QCOMPARE(typedSingle.value().value(), std::to_string(sequence + 4));
    ApplicationServicesCalendarEventImportSavePort importPort(services);
    const auto imported = importPort.saveImportedEvents(validImportSaveRequest());
    QVERIFY(imported);
    QCOMPARE(imported.value().size(), std::size_t(2));
    QCOMPARE(imported.value().at(0).value(), std::to_string(sequence + 5));
    QCOMPARE(imported.value().at(1).value(), std::to_string(sequence + 6));
    ApplicationServicesCalendarEventSeriesCreatePort seriesPort(services);
    auto request = seriesCreateRequest("series-edit", {
        {QDate(2026,12,8), QDate(2026,12,8)},
        {QDate(2026,12,9), QDate(2026,12,9)}
    }, QStringLiteral("Wide series"), QStringLiteral("Meeting"), QStringLiteral("Timed"));
    const auto series = seriesPort.createRepeatSeries(request);
    QVERIFY(series);
    QCOMPARE(series.value().size(), std::size_t(2));
    QCOMPARE(series.value().at(0).value(), std::to_string(sequence + 7));
    QCOMPARE(series.value().at(1).value(), std::to_string(sequence + 8));
    const auto editRequest = validSeriesEditRequest();
    const auto plan = planCalendarEventSeriesEditSuffix(editRequest, {
        {
            .eventId = calendarEventId(sequence + 7),
            .startDate = gregorianDate("2026-12-08")
        },
        {
            .eventId = calendarEventId(sequence + 8),
            .startDate = gregorianDate("2026-12-09")
        }
    });
    QVERIFY(plan);
    QVERIFY(plan.value().at(0).eventId == calendarEventId(sequence + 7));
    QVERIFY(plan.value().at(1).eventId == calendarEventId(sequence + 8));
    ApplicationServicesCalendarEventSeriesEditPort editPort(services);
    QVERIFY(editPort.editRepeatSeriesFromDate(editRequest));
    for (qint64 offset = 7; offset <= 8; ++offset)
    {
        const auto occurrence = repository->getCalendarEvent(sequence + offset);
        QVERIFY(occurrence);
        QCOMPARE(occurrence->id, sequence + offset);
        QCOMPARE(occurrence->title, QString::fromStdString(editRequest.title));
        QCOMPARE(occurrence->startDate, QDate(2026,12,10).addDays(offset - 7));
    }
    QVERIFY(query.exec("SELECT id, typeof(id) FROM calendar_events WHERE id > 1 ORDER BY id"));
    qint64 expected = sequence + 1;
    while (query.next())
    {
        QCOMPARE(query.value(0).toLongLong(), expected++);
        QCOMPARE(query.value(1).toString(), QStringLiteral("integer"));
    }
    QCOMPARE(expected, sequence + 9);
    query.finish();

    // Later insert failures roll back the earlier wide batch insert and sequence.
    QVERIFY(query.exec("CREATE TRIGGER reject_insert BEFORE INSERT ON calendar_events "
                       "WHEN NEW.title='Rejected' BEGIN SELECT RAISE(ABORT, 'failure'); END"));
    CalendarEvent rejected = event;
    rejected.title = QStringLiteral("Rejected");
    QVERIFY(!repository->saveCalendarEvent(rejected));
    QVERIFY(!repository->saveCalendarEvents({event, rejected}));
    auto rejectedRequest = validSaveRequest();
    rejectedRequest.title = "Rejected";
    verifyFailure(savePort.saveEvent(rejectedRequest), ErrorCode::Technical);
    auto importRequest = validImportSaveRequest();
    importRequest.events.back().title = "Rejected";
    verifyFailure(importPort.saveImportedEvents(importRequest), ErrorCode::Technical);
    QVERIFY(query.exec("DROP TRIGGER reject_insert"));
    const auto afterFailure = repository->saveCalendarEvent(event);
    QVERIFY(afterFailure);
    QCOMPARE(*afterFailure, sequence + 9);
    QCOMPARE(repository->getCalendarEvent(1)->title, QStringLiteral("Low"));
}

void NextPlatformApplicationServicesCalendarEventPortTests::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsCalendarEventPortAvailability()
{
    ApplicationServices services;
    ApplicationServicesCalendarEventPort port(services);
    ApplicationServicesCalendarEventImportSignatureQueryPort importPort(
        services
        );

    QVERIFY(!port.isAvailable());
    QVERIFY(!importPort.isAvailable());
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(port.isAvailable());
    QVERIFY(importPort.isAvailable());

    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!port.isAvailable());
    QVERIFY(!importPort.isAvailable());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
readsTwoCalendarYearsOfSubPrepIntervalsWithoutProjectionTruncation()
{
    constexpr int matchingEvents = 4'097;
    QDate requestedStartDate;
    QDate requestedEndDate;
    ApplicationServicesSubPrepCalendarEventIntervalsPort port(
        [&](const QDate& startDate, const QDate& endDate)
        {
            requestedStartDate = startDate;
            requestedEndDate = endDate;

            QList<CalendarEventDateInterval> intervals;
            intervals.reserve(matchingEvents + 4);
            intervals.append({
                .eventType = QStringLiteral("Vacation"),
                .startDate = QDate(2025, 12, 20),
                .endDate = QDate(2026, 1, 7)
            });
            for (int index = 0; index < matchingEvents; ++index)
            {
                intervals.append({
                    .eventType = QStringLiteral("Vacation"),
                    .startDate = QDate(2026, 7, 1),
                    .endDate = QDate(2026, 7, 29)
                });
            }
            intervals.append({
                .eventType = QStringLiteral("Holiday"),
                .startDate = QDate(2027, 12, 31),
                .endDate = QDate(2028, 1, 7)
            });
            intervals.append({
                .eventType = QStringLiteral("Vacation"),
                .startDate = QDate(2025, 12, 1),
                .endDate = QDate(2025, 12, 15)
            });
            intervals.append({
                .eventType = QStringLiteral("Holiday"),
                .startDate = QDate(2028, 1, 1),
                .endDate = QDate(2028, 1, 15)
            });
            return ::Result<QList<CalendarEventDateInterval>>(std::move(intervals));
        }
        );

    const auto result = SubPrepCalendarEventIntervalsQuery(port).execute({
        CalendarEventDate("2026-07-01")
        });

    QVERIFY(result);
    QCOMPARE(requestedStartDate, QDate(2026, 1, 1));
    QCOMPARE(requestedEndDate, QDate(2027, 12, 31));
    QCOMPARE(result.value().size(), std::size_t(matchingEvents + 2));
    QVERIFY(std::any_of(
        result.value().cbegin(),
        result.value().cend(),
        [](const SubPrepCalendarEventInterval& interval)
        {
            return interval.startDate.value() == "2025-12-20"
                && interval.endDate.value() == "2026-01-07";
        }
        ));
    QVERIFY(std::any_of(
        result.value().cbegin(),
        result.value().cend(),
        [](const SubPrepCalendarEventInterval& interval)
        {
            return interval.eventType == CalendarEventType::Holiday
                && interval.startDate.value() == "2027-12-31"
                && interval.endDate.value() == "2028-01-07";
        }
        ));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
readsTwoYearSubPrepIntervalsThroughApplicationServices()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* calendarService = services.calendarService();
    QVERIFY(calendarService);
    QVERIFY(calendarService->isAvailable());

    DataService* dataService = services.dataService();
    QVERIFY(dataService);
    DatabaseSession* session = dataService->databaseSession();
    QVERIFY(session);
    QSqlDatabase database = session->database();

    struct StoredInterval final
    {
        QString title;
        QString eventType;
        QDate startDate;
        QDate endDate;
    };
    const QList<StoredInterval> storedIntervals{
        {
            QStringLiteral("Before current year"),
            QStringLiteral("Vacation"),
            QDate(2025, 12, 1),
            QDate(2025, 12, 31)
        },
        {
            QStringLiteral("Historical prefix through window start"),
            QStringLiteral("Vacation"),
            QDate(2025, 12, 20),
            QDate(2026, 1, 1)
        },
        {
            QStringLiteral("Current year"),
            QStringLiteral("Vacation"),
            QDate(2026, 7, 10),
            QDate(2026, 7, 12)
        },
        {
            QStringLiteral("Following year boundary"),
            QStringLiteral("Holiday"),
            QDate(2027, 12, 31),
            QDate(2028, 1, 7)
        },
        {
            QStringLiteral("After following year"),
            QStringLiteral("Holiday"),
            QDate(2028, 1, 1),
            QDate(2028, 1, 15)
        }
    };

    for (const StoredInterval& interval : storedIntervals)
    {
        QSqlQuery insert(database);
        insert.prepare(R"(
            INSERT INTO calendar_events (
                title,
                event_type,
                start_date,
                end_date
            ) VALUES (?, ?, ?, ?)
        )");
        insert.addBindValue(interval.title);
        insert.addBindValue(interval.eventType);
        insert.addBindValue(interval.startDate.toString(Qt::ISODate));
        insert.addBindValue(interval.endDate.toString(Qt::ISODate));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }

    ApplicationServicesSubPrepCalendarEventIntervalsPort port(services);
    const auto result = SubPrepCalendarEventIntervalsQuery(port).execute({
        CalendarEventDate("2026-07-01")
    });

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(3));
    QVERIFY(result.value()[0].eventType == CalendarEventType::Vacation);
    QCOMPARE(result.value()[0].startDate.value(), std::string("2025-12-20"));
    QCOMPARE(result.value()[0].endDate.value(), std::string("2026-01-01"));
    QVERIFY(result.value()[1].eventType == CalendarEventType::Vacation);
    QCOMPARE(result.value()[1].startDate.value(), std::string("2026-07-10"));
    QCOMPARE(result.value()[1].endDate.value(), std::string("2026-07-12"));
    QVERIFY(result.value()[2].eventType == CalendarEventType::Holiday);
    QCOMPARE(result.value()[2].startDate.value(), std::string("2027-12-31"));
    QCOMPARE(result.value()[2].endDate.value(), std::string("2028-01-07"));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableSubPrepIntervalSessionsStructurally()
{
    ApplicationServices unopenedServices;
    QVERIFY(unopenedServices.dataService());
    ApplicationServicesSubPrepCalendarEventIntervalsPort unopenedPort(
        unopenedServices
        );
    verifyFailure(
        SubPrepCalendarEventIntervalsQuery(unopenedPort).execute({
            CalendarEventDate("2026-07-01")
        }),
        ErrorCode::NotFound
        );

    ApplicationServices closedServices;
    QVERIFY(openDatabase(closedServices, m_directory));
    closedServices.closeDatabase();
    QVERIFY(!closedServices.hasOpenDatabase());

    ApplicationServicesSubPrepCalendarEventIntervalsPort closedPort(
        closedServices
        );
    verifyFailure(
        SubPrepCalendarEventIntervalsQuery(closedPort).execute({
            CalendarEventDate("2026-07-01")
        }),
        ErrorCode::NotFound
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsSubPrepIntervalRepositoryFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(
        query.exec(QStringLiteral("DROP TABLE calendar_events")),
        qPrintable(query.lastError().text())
        );

    ApplicationServicesSubPrepCalendarEventIntervalsPort port(services);
    verifyFailure(
        SubPrepCalendarEventIntervalsQuery(port).execute({
            CalendarEventDate("2026-07-01")
        }),
        ErrorCode::Technical
        );
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
    QCOMPARE(events.at(0).eventType.value(), std::string("Workshop"));
    QCOMPARE(events.at(0).timeStatus, CalendarEventTimeStatus::Timed);
    QVERIFY(events.at(0).repeatSeriesId.has_value());
    QCOMPARE(events.at(0).repeatSeriesId->value(), std::string("series-july"));
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
    QCOMPARE(events.at(1).eventType.value(), std::string("Meeting"));
    QCOMPARE(events.at(1).timeStatus, CalendarEventTimeStatus::Timed);
    QCOMPARE(events.at(1).repeatSeriesId->value(), std::string("series-july"));
    QCOMPARE(events.at(1).order, std::int32_t(1));

    QCOMPARE(events.at(2).id.value(), std::to_string(overlapsEndId));
    QCOMPARE(events.at(2).title, std::string("Overlap end"));
    QCOMPARE(events.at(2).eventType.value(), std::string("Holiday"));
    QCOMPARE(events.at(2).timeStatus, CalendarEventTimeStatus::Timed);
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

    CalendarEventRepository* const repository =
        services.databaseSession()->calendarEventRepository();
    QVERIFY(repository);
    const CalendarEventByIdLookupResult lookup =
        repository->lookupCalendarEventById(eventId);
    QCOMPARE(lookup.code, CalendarEventByIdLookupCode::Found);
    QCOMPARE(lookup.event.id, eventId);
    QCOMPARE(lookup.event.title, QStringLiteral("Activation event"));

    ApplicationServicesCalendarEventPort port(services);
    const auto result = port.loadEventById(calendarEventId(eventId));

    QVERIFY(result);
    const CalendarEventSummary& projected = result.value();
    QCOMPARE(projected.id.value(), std::to_string(eventId));
    QCOMPARE(projected.title, std::string("Activation event"));
    QCOMPARE(projected.eventType.value(), std::string("Workshop"));
    QCOMPARE(projected.timeStatus, CalendarEventTimeStatus::Unconfirmed);
    QCOMPARE(projected.repeatSeriesId, std::optional<CalendarEventSeriesId>(
        calendarEventSeriesId("series-activation")
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
titleAdaptersUseUtf16UnitBoundariesForSqlReadAndWrite()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    const QDate eventDate(2026, 12, 10);

    ApplicationServicesCalendarEventSavePort savePort(services);
    CalendarEventSaveRequest acceptedRequest = validSaveRequest();
    const std::string acceptedTitle = repeatedHangulTitle(255);
    QCOMPARE(acceptedTitle.size(), std::size_t(765));
    acceptedRequest.title = acceptedTitle;
    const auto saved = savePort.saveEvent(acceptedRequest);
    QVERIFY(saved);

    bool idConverted = false;
    const int legacyId = QString::fromStdString(saved.value().value()).toInt(
        &idConverted
        );
    QVERIFY(idConverted);
    QVERIFY(legacyId > 0);
    CalendarEventRepository* const repository =
        services.databaseSession()->calendarEventRepository();
    QVERIFY(repository);
    const auto stored = repository->getCalendarEvent(legacyId);
    QVERIFY(stored);
    QCOMPARE(stored->title.toUtf8().toStdString(), acceptedTitle);

    ApplicationServicesCalendarEventPort readPort(services);
    const auto projected = readPort.projection(eventDate, eventDate);
    QVERIFY(projected);
    QCOMPARE(projected.value().eventCount(), std::size_t(1));
    QCOMPARE(projected.value().events().front().title, acceptedTitle);

    CalendarEventSaveRequest rejectedRequest = validSaveRequest();
    rejectedRequest.title = repeatedHangulTitle(256);
    verifyFailure(
        savePort.saveEvent(rejectedRequest),
        ErrorCode::InvalidInput
        );

    QSqlQuery rawInsert(
        services.dataService()->databaseSession()->database()
        );
    rawInsert.prepare(QStringLiteral(
        "INSERT INTO calendar_events ("
        "title, event_type, time_status, repeat_series_id, all_day, "
        "start_date, start_time, end_date, end_time) "
        "VALUES (?, 'Other', 'Timed', NULL, 0, ?, ?, ?, ?)"
        ));
    rawInsert.addBindValue(QString(256, QChar(0xAC00)));
    rawInsert.addBindValue(eventDate.toString(Qt::ISODate));
    rawInsert.addBindValue(QStringLiteral("09:00"));
    rawInsert.addBindValue(eventDate.toString(Qt::ISODate));
    rawInsert.addBindValue(QStringLiteral("10:00"));
    QVERIFY(rawInsert.exec());

    const auto rejectedProjection = readPort.projection(eventDate, eventDate);
    verifyFailure(rejectedProjection, ErrorCode::InvalidInput);
}

void NextPlatformApplicationServicesCalendarEventPortTests::
deletesValidTypedEvent()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    CalendarEvent event = makeEvent(
        QStringLiteral("Delete me"),
        QDate(2026, 12, 1),
        QDate(2026, 12, 1)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const auto saved = repository->saveCalendarEvent(event);
    QVERIFY(saved);
    const int eventId = *saved;
    QVERIFY(eventId > 0);
    QVERIFY(repository->getCalendarEvent(eventId));

    ApplicationServicesCalendarEventDeletePort port(services);
    const auto deleted = port.deleteEvent(calendarEventId(eventId));

    QVERIFY(deleted);
    QVERIFY(!repository->getCalendarEvent(eventId));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
deletesMissingValidTypedEventSuccessfully()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);
    QVERIFY(!repository->getCalendarEvent(1));

    ApplicationServicesCalendarEventDeletePort port(services);
    const auto deleted = port.deleteEvent(calendarEventId(1));

    QVERIFY(deleted);
    QVERIFY(!repository->getCalendarEvent(1));
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
reportsUnavailableDeleteSessionStructurally()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());

    ApplicationServicesCalendarEventDeletePort port(services);

    verifyRecoverableFailure(
        port.deleteEvent(calendarEventId(1)),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );

    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(session->isOpen());
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());

    verifyRecoverableFailure(
        port.deleteEvent(calendarEventId(1)),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsDeleteServiceFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    CalendarEvent event = makeEvent(
        QStringLiteral("Delete failure"),
        QDate(2026, 12, 2),
        QDate(2026, 12, 2)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const auto saved = repository->saveCalendarEvent(event);
    QVERIFY(saved);
    const int eventId = *saved;
    QVERIFY(eventId > 0);

    QSqlQuery query(session->database());
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
    QVERIFY(repository->getCalendarEvent(eventId));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
deletesAllCalendarEventsThroughTypedPort()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    CalendarEvent firstEvent = makeEvent(
        QStringLiteral("Reset one"),
        QDate(2026, 12, 2),
        QDate(2026, 12, 2)
        );
    firstEvent.startTime = QTime(9, 0);
    firstEvent.endTime = QTime(10, 0);
    const auto firstSaved = repository->saveCalendarEvent(firstEvent);
    QVERIFY(firstSaved);
    QVERIFY(*firstSaved > 0);

    CalendarEvent secondEvent = makeEvent(
        QStringLiteral("Reset two"),
        QDate(2026, 12, 3),
        QDate(2026, 12, 3)
        );
    secondEvent.startTime = QTime(11, 0);
    secondEvent.endTime = QTime(12, 0);
    secondEvent.repeatSeriesId = QStringLiteral("reset-series");
    const auto secondSaved = repository->saveCalendarEvent(secondEvent);
    QVERIFY(secondSaved);
    QVERIFY(*secondSaved > *firstSaved);
    QVERIFY(repository->getCalendarEvent(*firstSaved));
    QVERIFY(repository->getCalendarEvent(*secondSaved));

    ApplicationServicesCalendarEventDeleteAllPort port(services);
    QVERIFY(port.isAvailable());
    const auto deleted = port.deleteAllEvents();
    QVERIFY(deleted);
    const auto remaining = repository->loadCalendarEventsInRange(
        QDate(2026, 12, 1),
        QDate(2026, 12, 31)
        );
    QVERIFY(remaining);
    QVERIFY(remaining->isEmpty());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableDeleteAllSessionStructurally()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());
    ApplicationServicesCalendarEventDeleteAllPort port(services);

    QVERIFY(!port.isAvailable());
    verifyRecoverableFailure(
        port.deleteAllEvents(),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );

    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(session->isOpen());
    QVERIFY(session->calendarEventRepository());
    QVERIFY(port.isAvailable());

    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    QVERIFY(!port.isAvailable());
    verifyRecoverableFailure(
        port.deleteAllEvents(),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsDeleteAllServiceFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    CalendarEvent event = makeEvent(
        QStringLiteral("Reset failure"),
        QDate(2026, 12, 2),
        QDate(2026, 12, 2)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const auto saved = repository->saveCalendarEvent(event);
    QVERIFY(saved);
    const int eventId = *saved;
    QVERIFY(eventId > 0);

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_delete_all "
        "BEFORE DELETE ON calendar_events "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar reset failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventDeleteAllPort port(services);
    const auto deleted = port.deleteAllEvents();

    verifyFailure(deleted, ErrorCode::Technical);
    QVERIFY(
        deleted.error().message.find("injected calendar reset failure")
            != std::string::npos
        || deleted.error().message.find("Deleting all calendar events")
            != std::string::npos
        );
    QVERIFY(repository->getCalendarEvent(eventId));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
savesOrderedCalendarImportBatchWithTypedIdsAndParity()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    CalendarEventImportSaveRequest request = validImportSaveRequest();
    request.events.front().title = "  Later   imported event  ";
    request.events.front().eventType = "  Workshop  ";
    request.events.front().timeStatus = CalendarEventTimeStatus::Unknown;
    CalendarEventSaveRequest timedEvent = validSaveRequest();
    timedEvent.title = "Timed imported event";
    timedEvent.startDate = gregorianDate("2026-12-11");
    timedEvent.endDate = gregorianDate("2026-12-11");
    request.events.push_back(std::move(timedEvent));

    ApplicationServicesCalendarEventImportSavePort port(services);
    const auto saved = port.saveImportedEvents(request);

    QVERIFY(saved);
    QCOMPARE(saved.value().size(), std::size_t(3));
    const int laterEventId = std::stoi(saved.value().at(0).value());
    const int earlierEventId = std::stoi(saved.value().at(1).value());
    const int timedEventId = std::stoi(saved.value().at(2).value());
    QVERIFY(laterEventId > 0);
    QVERIFY(earlierEventId > laterEventId);
    QVERIFY(timedEventId > earlierEventId);

    const auto laterEvent = repository->getCalendarEvent(laterEventId);
    QVERIFY(laterEvent);
    QCOMPARE(laterEvent->title, QStringLiteral("Later imported event"));
    QCOMPARE(laterEvent->eventType, QStringLiteral("Workshop"));
    QCOMPARE(laterEvent->startDate, QDate(2026, 12, 12));
    QCOMPARE(laterEvent->endDate, QDate(2026, 12, 12));
    QCOMPARE(laterEvent->timeStatus, QStringLiteral("Unknown"));
    QVERIFY(!laterEvent->allDay);
    QVERIFY(!laterEvent->startTime.isValid());
    QVERIFY(!laterEvent->endTime.isValid());
    QVERIFY(laterEvent->repeatSeriesId.isEmpty());

    const auto earlierEvent = repository->getCalendarEvent(earlierEventId);
    QVERIFY(earlierEvent);
    QCOMPARE(earlierEvent->title, QStringLiteral("Earlier imported event"));
    QCOMPARE(earlierEvent->startDate, QDate(2026, 12, 10));
    QCOMPARE(earlierEvent->endDate, QDate(2026, 12, 10));
    QCOMPARE(earlierEvent->eventType, QStringLiteral("Holiday"));
    QCOMPARE(earlierEvent->timeStatus, QStringLiteral("Timed"));
    QVERIFY(earlierEvent->allDay);
    QVERIFY(!earlierEvent->startTime.isValid());
    QVERIFY(!earlierEvent->endTime.isValid());
    QVERIFY(earlierEvent->repeatSeriesId.isEmpty());

    const auto persistedTimedEvent =
        repository->getCalendarEvent(timedEventId);
    QVERIFY(persistedTimedEvent);
    QCOMPARE(
        persistedTimedEvent->title,
        QStringLiteral("Timed imported event")
        );
    QCOMPARE(persistedTimedEvent->eventType, QStringLiteral("Meeting"));
    QCOMPARE(persistedTimedEvent->startDate, QDate(2026, 12, 11));
    QCOMPARE(persistedTimedEvent->endDate, QDate(2026, 12, 11));
    QCOMPARE(persistedTimedEvent->timeStatus, QStringLiteral("Timed"));
    QVERIFY(!persistedTimedEvent->allDay);
    QCOMPARE(persistedTimedEvent->startTime, QTime(9, 0));
    QCOMPARE(persistedTimedEvent->endTime, QTime(10, 0));
    QVERIFY(persistedTimedEvent->repeatSeriesId.isEmpty());

    const auto loaded = repository->loadCalendarEventsInRange(
        QDate(2026, 12, 10),
        QDate(2026, 12, 12)
        );
    QVERIFY(loaded);
    QCOMPARE(loaded->size(), 3);
}

void NextPlatformApplicationServicesCalendarEventPortTests::
acceptsDuplicateOnlyEmptyImportBatchAsNoOp()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->calendarEventRepository());
    ApplicationServicesCalendarEventImportSavePort port(services);

    const auto saved = port.saveImportedEvents({});
    QVERIFY(saved);
    QVERIFY(saved.value().empty());
    const auto loaded = session->calendarEventRepository()
        ->loadCalendarEventsInRange(
            QDate(2026, 12, 10),
            QDate(2026, 12, 12)
            );
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
rejectsInvalidImportBatchBeforeCreatingAnyRows()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    auto request = validImportSaveRequest();
    request.events.back().id = calendarEventId(999);
    ApplicationServicesCalendarEventImportSavePort port(services);
    const auto creationOnly = port.saveImportedEvents(request);
    verifyFailure(creationOnly, ErrorCode::InvalidInput);
    QCOMPARE(
        creationOnly.error().message,
        std::string("Calendar imports may only create new events.")
        );

    CalendarEventImportSaveRequest tooManyEvents;
    tooManyEvents.events.resize(
        kCalendarEventImportSaveMaxEvents + 1,
        validSaveRequest()
        );
    const auto tooMany = port.saveImportedEvents(tooManyEvents);
    verifyFailure(tooMany, ErrorCode::InvalidInput);
    QCOMPARE(
        tooMany.error().message,
        std::string("Calendar import event count exceeds the maximum.")
        );

    const auto loaded = repository->loadCalendarEventsInRange(
        QDate(2026, 12, 10),
        QDate(2026, 12, 12)
        );
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsImportBatchFailureWithoutPartialRows()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_import_second "
        "BEFORE INSERT ON calendar_events "
        "WHEN NEW.title = 'Rejected imported event' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar import save failure'); "
        "END"
        )));

    auto request = validImportSaveRequest();
    request.events.back().title = "Rejected imported event";
    ApplicationServicesCalendarEventImportSavePort port(services);
    const auto saved = port.saveImportedEvents(request);

    verifyFailure(saved, ErrorCode::Technical);
    QVERIFY(
        saved.error().message.find("injected calendar import save failure")
            != std::string::npos
        || saved.error().message.find("Creating calendar event")
            != std::string::npos
    );

    const auto loaded = repository->loadCalendarEventsInRange(
        QDate(2026, 12, 10),
        QDate(2026, 12, 12)
        );
    QVERIFY(loaded);
    QVERIFY(loaded->isEmpty());
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableImportSaveSessionStructurally()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());
    ApplicationServicesCalendarEventImportSavePort port(services);

    verifyRecoverableFailure(
        port.saveImportedEvents(validImportSaveRequest()),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );
    verifyRecoverableFailure(
        port.saveImportedEvents({}),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );

    auto invalidRequest = validImportSaveRequest();
    invalidRequest.events.front().id = calendarEventId(999);
    verifyFailure(
        port.saveImportedEvents(invalidRequest),
        ErrorCode::InvalidInput
        );

    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(session->isOpen());
    QVERIFY(session->calendarEventRepository());
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());

    verifyRecoverableFailure(
        port.saveImportedEvents(validImportSaveRequest()),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );
    verifyRecoverableFailure(
        port.saveImportedEvents({}),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
createsAndUpdatesValidEventWithTypedIdMapping()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    ApplicationServicesCalendarEventSavePort port(services);
    CalendarEventSaveRequest createRequest = validSaveRequest();
    createRequest.title = "  Typed   save event  ";
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
    const auto loadedCreated = repository->getCalendarEvent(legacyId);
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
    updateRequest.startDate = gregorianDate("2026-12-11");
    updateRequest.endDate = gregorianDate("2026-12-12");
    updateRequest.startTime = "11:00";
    updateRequest.endTime = "12:30";
    const auto updated = port.saveEvent(updateRequest);
    QVERIFY(updated);
    QCOMPARE(updated.value(), createdId);

    const auto loadedUpdated = repository->getCalendarEvent(legacyId);
    QVERIFY(loadedUpdated);
    QCOMPARE(loadedUpdated->id, legacyId);
    QCOMPARE(loadedUpdated->title, QStringLiteral("Typed updated event"));
    QCOMPARE(loadedUpdated->startDate, QDate(2026, 12, 11));
    QCOMPARE(loadedUpdated->endDate, QDate(2026, 12, 12));
    QCOMPARE(loadedUpdated->startTime, QTime(11, 0));
    QCOMPARE(loadedUpdated->endTime, QTime(12, 30));
    const auto loadedRange = repository->loadCalendarEventsInRange(
            QDate(2026, 12, 10),
            QDate(2026, 12, 12)
            );
    QVERIFY(loadedRange);
    QCOMPARE(loadedRange->size(), 1);
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsMissingCalendarEventUpdateStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const auto created = repository->saveCalendarEvent(
        makeEvent(
            QStringLiteral("Deleted before update"),
            QDate(2026, 12, 10),
            QDate(2026, 12, 10)
            )
        );
    QVERIFY(created);
    const int eventId = *created;
    QVERIFY(eventId > 0);
    QVERIFY(repository->deleteCalendarEvent(eventId));

    CalendarEvent legacyUpdate = makeEvent(
        QStringLiteral("Legacy missing update"),
        QDate(2026, 12, 10),
        QDate(2026, 12, 10)
        );
    legacyUpdate.id = eventId;
    const auto legacySaved = repository->saveCalendarEvent(legacyUpdate);
    QVERIFY(!legacySaved);
    const QString expectedDiagnostic =
        QStringLiteral(
            "Updating calendar event id %1 failed: no matching record exists."
            ).arg(eventId);
    QCOMPARE(legacySaved.error(), expectedDiagnostic);

    ApplicationServicesCalendarEventSavePort port(services);
    CalendarEventSaveRequest updateRequest = validSaveRequest();
    updateRequest.id = calendarEventId(eventId);
    const auto updated = port.saveEvent(updateRequest);

    verifyFailure(updated, ErrorCode::NotFound);
    QCOMPARE(
        updated.error().message,
        expectedDiagnostic.toUtf8().toStdString()
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsCalendarEventUpdateSqlFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const auto created = repository->saveCalendarEvent(
        makeEvent(
            QStringLiteral("Update failure"),
            QDate(2026, 12, 10),
            QDate(2026, 12, 10)
            )
        );
    QVERIFY(created);
    const int eventId = *created;
    QVERIFY(eventId > 0);

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_update "
        "BEFORE UPDATE ON calendar_events "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar update failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventSavePort port(services);
    CalendarEventSaveRequest updateRequest = validSaveRequest();
    updateRequest.id = calendarEventId(eventId);
    updateRequest.title = "Updated value";
    const auto updated = port.saveEvent(updateRequest);

    verifyFailure(updated, ErrorCode::Technical);
    QVERIFY(
        updated.error().message.find("injected calendar update failure")
            != std::string::npos
        );
    const auto stillStored = repository->getCalendarEvent(eventId);
    QVERIFY(stillStored);
    QCOMPARE(stillStored->title, QStringLiteral("Update failure"));
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

    auto reversedDateRange = validSaveRequest();
    reversedDateRange.startDate = gregorianDate("2026-12-11");
    verifyFailure(port.saveEvent(reversedDateRange), ErrorCode::InvalidInput);

    auto invalidStatus = validSaveRequest();
    invalidStatus.timeStatus = static_cast<CalendarEventTimeStatus>(99);
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
rejectsPaddedAndMalformedTypedTimesAtPublicBoundary_data()
{
    QTest::addColumn<int>("operation");
    QTest::addColumn<QString>("invalidTime");
    QTest::addColumn<bool>("startTime");

    QTest::newRow("save-padded-start")
        << static_cast<int>(CalendarEventWriteOperation::Save)
        << QStringLiteral(" 09:15")
        << true;
    QTest::newRow("save-malformed-end")
        << static_cast<int>(CalendarEventWriteOperation::Save)
        << QStringLiteral("10:60")
        << false;
    QTest::newRow("import-padded-start")
        << static_cast<int>(CalendarEventWriteOperation::ImportSave)
        << QStringLiteral(" 09:15")
        << true;
    QTest::newRow("import-malformed-end")
        << static_cast<int>(CalendarEventWriteOperation::ImportSave)
        << QStringLiteral("10:60")
        << false;
    QTest::newRow("series-create-padded-start")
        << static_cast<int>(CalendarEventWriteOperation::SeriesCreate)
        << QStringLiteral(" 09:15")
        << true;
    QTest::newRow("series-create-malformed-end")
        << static_cast<int>(CalendarEventWriteOperation::SeriesCreate)
        << QStringLiteral("10:60")
        << false;
    QTest::newRow("series-edit-padded-start")
        << static_cast<int>(CalendarEventWriteOperation::SeriesEdit)
        << QStringLiteral(" 09:15")
        << true;
    QTest::newRow("series-edit-malformed-end")
        << static_cast<int>(CalendarEventWriteOperation::SeriesEdit)
        << QStringLiteral("10:60")
        << false;
}

void NextPlatformApplicationServicesCalendarEventPortTests::
rejectsPaddedAndMalformedTypedTimesAtPublicBoundary()
{
    QFETCH(int, operation);
    QFETCH(QString, invalidTime);
    QFETCH(bool, startTime);

    const auto operationKind =
        static_cast<CalendarEventWriteOperation>(operation);
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const QString repeatSeriesId =
        QStringLiteral("series-invalid-public-time-boundary");
    CalendarEvent original = makeEvent(
        QStringLiteral("Untouched boundary fixture"),
        QDate(2026, 12, 10),
        QDate(2026, 12, 10)
        );
    original.eventType = QStringLiteral("Meeting");
    original.timeStatus = QStringLiteral("Timed");
    original.startTime = QTime(8, 0);
    original.endTime = QTime(9, 0);
    original.repeatSeriesId = repeatSeriesId;
    const auto seeded = repository->saveCalendarEvent(original);
    QVERIFY(seeded);
    const int originalEventId = *seeded;
    const auto seededEvent = repository->getCalendarEvent(originalEventId);
    QVERIFY(seededEvent);
    original = *seededEvent;

    const QString triggerSql = operationKind == CalendarEventWriteOperation::SeriesEdit
        ? QStringLiteral(
            "CREATE TRIGGER reject_invalid_typed_time_update "
            "BEFORE UPDATE ON calendar_events "
            "BEGIN SELECT RAISE(ABORT, 'invalid typed time reached update'); END"
            )
        : QStringLiteral(
            "CREATE TRIGGER reject_invalid_typed_time_insert "
            "BEFORE INSERT ON calendar_events "
            "BEGIN SELECT RAISE(ABORT, 'invalid typed time reached insert'); END"
            );
    QSqlQuery persistenceGuard(session->database());
    QVERIFY2(
        persistenceGuard.exec(triggerSql),
        qPrintable(persistenceGuard.lastError().text())
        );

    QSqlQuery countBefore(session->database());
    QVERIFY(countBefore.exec(QStringLiteral(
        "SELECT COUNT(*) FROM calendar_events"
        )));
    QVERIFY(countBefore.next());
    const int originalRowCount = countBefore.value(0).toInt();
    QCOMPARE(originalRowCount, 1);

    // These public-port cases assert typed request validation. The requests
    // reject before private legacyTime helpers, so this is not helper-path
    // parsing coverage.
    const std::string expectedMessage =
        operationKind == CalendarEventWriteOperation::SeriesEdit
            ? "Calendar repeat-series edit times must be valid canonical 24-hour times."
            : "Calendar event times must be valid canonical 24-hour times.";

    switch (operationKind)
    {
    case CalendarEventWriteOperation::Save:
    {
        CalendarEventSaveRequest request = validSaveRequest();
        request.title = "Rejected non-canonical typed time";
        setInvalidTime(request, invalidTime, startTime);
        const auto requestValidation = request.validate();
        ApplicationServicesCalendarEventSavePort port(services);
        const auto savedResult = port.saveEvent(request);
        verifyPublicTimeBoundaryFailure(
            requestValidation,
            savedResult,
            expectedMessage
            );
        break;
    }

    case CalendarEventWriteOperation::ImportSave:
    {
        CalendarEventImportSaveRequest request = validImportSaveRequest();
        request.events.front().startTime = "09:15";
        request.events.front().endTime = "10:45";
        setInvalidTime(request.events.front(), invalidTime, startTime);
        const auto requestValidation = request.validate();
        ApplicationServicesCalendarEventImportSavePort port(services);
        const auto savedResult = port.saveImportedEvents(request);
        verifyPublicTimeBoundaryFailure(
            requestValidation,
            savedResult,
            expectedMessage
            );
        break;
    }

    case CalendarEventWriteOperation::SeriesCreate:
    {
        CalendarEventSeriesCreateRequest request = seriesCreateRequest(
            "series-invalid-typed-time",
            {{QDate(2026, 12, 15), QDate(2026, 12, 15)}},
            QStringLiteral("Rejected non-canonical typed time"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed")
            );
        setInvalidTime(request.occurrences.front(), invalidTime, startTime);
        const auto requestValidation = request.validate();
        ApplicationServicesCalendarEventSeriesCreatePort port(services);
        const auto savedResult = port.createRepeatSeries(request);
        verifyPublicTimeBoundaryFailure(
            requestValidation,
            savedResult,
            expectedMessage
            );
        break;
    }

    case CalendarEventWriteOperation::SeriesEdit:
    {
        CalendarEventSeriesEditRequest request = validSeriesEditRequest();
        request.repeatSeriesId = calendarEventSeriesId(
            repeatSeriesId.toStdString()
            );
        if (startTime)
        {
            request.startTime = invalidTime.toStdString();
        }
        else
        {
            request.endTime = invalidTime.toStdString();
        }
        const auto requestValidation = request.validate();
        ApplicationServicesCalendarEventSeriesEditPort port(services);
        const auto savedResult = port.editRepeatSeriesFromDate(request);
        verifyPublicTimeBoundaryFailure(
            requestValidation,
            savedResult,
            expectedMessage
            );
        break;
    }
    }

    QSqlQuery countAfter(session->database());
    QVERIFY(countAfter.exec(QStringLiteral(
        "SELECT COUNT(*) FROM calendar_events"
        )));
    QVERIFY(countAfter.next());
    QCOMPARE(countAfter.value(0).toInt(), originalRowCount);

    const auto unchanged = repository->getCalendarEvent(originalEventId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->title, original.title);
    QCOMPARE(unchanged->startDate, original.startDate);
    QCOMPARE(unchanged->endDate, original.endDate);
    QCOMPARE(unchanged->startTime, original.startTime);
    QCOMPARE(unchanged->endTime, original.endTime);
    QCOMPARE(unchanged->repeatSeriesId, original.repeatSeriesId);
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableSaveSessionStructurally()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());

    ApplicationServicesCalendarEventSavePort port(services);

    auto invalidUpdateId = validSaveRequest();
    invalidUpdateId.id = *CalendarEventId::fromString("0");
    verifyFailure(
        port.saveEvent(invalidUpdateId),
        ErrorCode::InvalidInput
        );

    const auto verifyUnavailableSave = [](
        const CalendarEventSaveResult& result
        )
    {
        QVERIFY(!result);
        QCOMPARE(result.error().code, ErrorCode::NotFound);
        QCOMPARE(
            result.error().message,
            std::string("The calendar service is unavailable.")
            );
        QVERIFY(result.error().recoverable);
    };

    auto validUpdate = validSaveRequest();
    validUpdate.id = calendarEventId(1);
    verifyUnavailableSave(
        CalendarEventSaveUseCase::execute(
            port,
            saveInputFromRequest(validUpdate)
            )
        );

    verifyUnavailableSave(
        CalendarEventSaveUseCase::execute(
            port,
            saveInputFromRequest(validSaveRequest())
            )
        );

    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(session->isOpen());
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());

    verifyFailure(
        port.saveEvent(invalidUpdateId),
        ErrorCode::InvalidInput
        );

    verifyUnavailableSave(
        CalendarEventSaveUseCase::execute(
            port,
            saveInputFromRequest(validSaveRequest())
            )
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsSaveServiceFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    QSqlQuery query(
        services.databaseSession()->database()
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
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    ApplicationServicesCalendarEventSeriesCreatePort port(services);
    QList<CalendarEventSeriesCreateRequest> requests{
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
    requests[0].repeatSeriesId = calendarEventSeriesId(" \tseries-daily \t");
    requests[0].occurrences[0].title = "  Daily   title  ";
    requests[0].occurrences[0].eventType = "  Meeting  ";
    for (CalendarEventSaveRequest& occurrence : requests[1].occurrences)
    {
        occurrence.eventType = "  Other  ";
    }

    const QList<QString> seriesIds{
        QStringLiteral("series-daily"),
        QStringLiteral("series-weekly"),
        QStringLiteral("series-monthly")
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

    int previousEventId = 0;
    for (int requestIndex = 0; requestIndex < requests.size();
         ++requestIndex)
    {
        const auto created = port.createRepeatSeries(
            requests.at(requestIndex)
            );
        QVERIFY(created);
        QCOMPARE(
            created.value().size(),
            requests.at(requestIndex).occurrences.size()
            );

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
            QVERIFY(legacyId > previousEventId);
            previousEventId = legacyId;

            const auto loaded = repository->getCalendarEvent(legacyId);
            QVERIFY(loaded);
            QCOMPARE(
                loaded->repeatSeriesId,
                seriesIds.at(requestIndex)
                );
            QCOMPARE(
                loaded->title,
                QString::fromStdString(
                    occurrences.at(occurrenceIndex).title
                    ).simplified()
                );
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
                    ).trimmed()
                );
            QCOMPARE(
                loaded->timeStatus,
                ClassMngr::Next::Platform::calendarEventTimeStatusToLegacyText(
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
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    ApplicationServicesCalendarEventSeriesCreatePort port(services);

    QSqlQuery countBefore(session->database());
    QVERIFY(countBefore.exec(QStringLiteral(
        "SELECT COUNT(*) FROM calendar_events"
        )));
    QVERIFY(countBefore.next());
    const int originalRowCount = countBefore.value(0).toInt();
    QCOMPARE(originalRowCount, 0);

    const std::vector<std::string> unicodeWhitespaceIds{
        "\xC2\xA0",
        "\xE3\x80\x80"
    };
    for (const std::string& repeatSeriesId : unicodeWhitespaceIds)
    {
        const auto result = port.createRepeatSeries(seriesCreateRequest(
            repeatSeriesId,
            {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
            QStringLiteral("Valid title"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed")
            ));
        verifyFailure(result, ErrorCode::InvalidInput);
    }

    QSqlQuery countAfterUnicodeWhitespace(session->database());
    QVERIFY(countAfterUnicodeWhitespace.exec(QStringLiteral(
        "SELECT COUNT(*) FROM calendar_events"
        )));
    QVERIFY(countAfterUnicodeWhitespace.next());
    QCOMPARE(countAfterUnicodeWhitespace.value(0).toInt(), originalRowCount);

    auto blankSeries = seriesCreateRequest(
        "series-invalid",
        {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
        QStringLiteral("Valid title"),
        QStringLiteral("Meeting"),
        QStringLiteral("Timed")
        );
    blankSeries.repeatSeriesId = calendarEventSeriesId(" \t");
    verifyFailure(
        port.createRepeatSeries(blankSeries),
        ErrorCode::InvalidInput
        );

    auto absentSeriesId = blankSeries;
    absentSeriesId.repeatSeriesId = CalendarEventSeriesId::fromString("");
    verifyFailure(
        port.createRepeatSeries(absentSeriesId),
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
    emptySeries.repeatSeriesId = calendarEventSeriesId("series-empty");
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
rejectsExistingOccurrenceIdBeforePersistence()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* const calendarService = services.calendarService();
    QVERIFY(calendarService);
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    CalendarEvent existingEvent = makeEvent(
        QStringLiteral("Existing standalone event"),
        QDate(2026, 12, 10),
        QDate(2026, 12, 10)
        );
    existingEvent.eventType = QStringLiteral("Meeting");
    existingEvent.timeStatus = QStringLiteral("Timed");
    existingEvent.startTime = QTime(9, 0);
    existingEvent.endTime = QTime(10, 0);
    const auto seeded = calendarService->saveEvent(existingEvent);
    QVERIFY(seeded);
    const int existingEventId = seeded.value();
    QVERIFY(existingEventId > 0);

    const auto seededRow = repository->getCalendarEvent(existingEventId);
    QVERIFY(seededRow);
    const CalendarEvent original = *seededRow;

    QSqlQuery countBefore(session->database());
    QVERIFY(countBefore.exec(QStringLiteral(
        "SELECT COUNT(*) FROM calendar_events"
        )));
    QVERIFY(countBefore.next());
    const int originalRowCount = countBefore.value(0).toInt();
    QCOMPARE(originalRowCount, 1);

    CalendarEventSeriesCreateRequest request = seriesCreateRequest(
        "series-with-existing-id",
        {
            {QDate(2026, 12, 15), QDate(2026, 12, 15)},
            {QDate(2026, 12, 22), QDate(2026, 12, 22)}
        },
        QStringLiteral("Attempted series replacement"),
        QStringLiteral("Workshop"),
        QStringLiteral("Timed")
        );
    request.occurrences.at(1).id = calendarEventId(existingEventId);

    verifyFailure(request.validate(), ErrorCode::InvalidInput);

    ApplicationServicesCalendarEventSeriesCreatePort port(services);
    verifyFailure(
        port.createRepeatSeries(request),
        ErrorCode::InvalidInput
        );

    QSqlQuery countAfter(session->database());
    QVERIFY(countAfter.exec(QStringLiteral(
        "SELECT COUNT(*) FROM calendar_events"
        )));
    QVERIFY(countAfter.next());
    QCOMPARE(countAfter.value(0).toInt(), originalRowCount);

    const auto remainingRow = repository->getCalendarEvent(existingEventId);
    QVERIFY(remainingRow);
    QCOMPARE(remainingRow->id, original.id);
    QCOMPARE(remainingRow->title, original.title);
    QCOMPARE(remainingRow->eventType, original.eventType);
    QCOMPARE(remainingRow->timeStatus, original.timeStatus);
    QCOMPARE(remainingRow->repeatSeriesId, original.repeatSeriesId);
    QCOMPARE(remainingRow->allDay, original.allDay);
    QCOMPARE(remainingRow->startDate, original.startDate);
    QCOMPARE(remainingRow->startTime, original.startTime);
    QCOMPARE(remainingRow->endDate, original.endDate);
    QCOMPARE(remainingRow->endTime, original.endTime);
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableSeriesCreateSessionStructurally()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());
    ApplicationServicesCalendarEventSeriesCreatePort port(services);

    auto invalidRequest = seriesCreateRequest(
        "series-unavailable",
        {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
        QStringLiteral("Valid title"),
        QStringLiteral("Meeting"),
        QStringLiteral("Timed")
        );
    invalidRequest.repeatSeriesId = calendarEventSeriesId(" \t");
    verifyFailure(
        port.createRepeatSeries(invalidRequest),
        ErrorCode::InvalidInput
        );

    verifyRecoverableFailure(
        port.createRepeatSeries(seriesCreateRequest(
            "series-unavailable",
            {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
            QStringLiteral("Valid title"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed")
            )),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );

    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(session->isOpen());
    QVERIFY(session->calendarEventRepository());
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    verifyRecoverableFailure(
        port.createRepeatSeries(seriesCreateRequest(
            "series-closed",
            {{QDate(2026, 12, 1), QDate(2026, 12, 1)}},
            QStringLiteral("Valid title"),
            QStringLiteral("Meeting"),
            QStringLiteral("Timed")
            )),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsSeriesCreateBatchFailureWithoutPartialRows()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    QSqlQuery query(session->database());
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

    const auto loaded = repository->loadCalendarEventsInRange(
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
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

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
        const auto saved = repository->saveCalendarEvent(event);
        QVERIFY(saved);
        const int eventId = *saved;
        QVERIFY(eventId > 0);
        eventIds.append(eventId);
    }

    CalendarEvent unrelatedEvent = makeEvent(
        QStringLiteral("Unrelated repeat event"),
        QDate(2026, 12, 15),
        QDate(2026, 12, 16)
        );
    unrelatedEvent.startTime = QTime(15, 0);
    unrelatedEvent.endTime = QTime(16, 0);
    unrelatedEvent.repeatSeriesId = QStringLiteral("series-other-edit");
    const auto unrelatedSaved = repository->saveCalendarEvent(unrelatedEvent);
    QVERIFY(unrelatedSaved);
    const int unrelatedEventId = *unrelatedSaved;
    QVERIFY(unrelatedEventId > 0);

    const auto before = repository->loadCalendarEventsForRepeatSeriesFromDate(
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
    request.repeatSeriesId = calendarEventSeriesId(" series-edit ");
    const auto edited = port.editRepeatSeriesFromDate(request);
    QVERIFY(edited);

    const auto after = repository->loadCalendarEventsForRepeatSeriesFromDate(
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

    const auto unrelated = repository->getCalendarEvent(unrelatedEventId);
    QVERIFY(unrelated);
    QCOMPARE(unrelated->title, QStringLiteral("Unrelated repeat event"));
    QCOMPARE(unrelated->repeatSeriesId, QStringLiteral("series-other-edit"));
    QCOMPARE(unrelated->startDate, QDate(2026, 12, 15));
    QCOMPARE(unrelated->endDate, QDate(2026, 12, 16));
    QCOMPARE(unrelated->startTime, QTime(15, 0));
    QCOMPARE(unrelated->endTime, QTime(16, 0));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
acceptsEmptyRepeatSeriesEditSuffixAsNoOp()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const QString repeatSeriesId = QStringLiteral("series-empty-edit-suffix");
    CalendarEvent event = makeEvent(
        QStringLiteral("Before selected suffix"),
        QDate(2026, 12, 1),
        QDate(2026, 12, 1)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    event.repeatSeriesId = repeatSeriesId;
    const auto saved = repository->saveCalendarEvent(event);
    QVERIFY(saved);
    const int eventId = *saved;
    QVERIFY(eventId > 0);

    const auto before = repository->loadCalendarEventsForRepeatSeriesFromDate(
        repeatSeriesId,
        QDate(2026, 12, 8)
        );
    QVERIFY(before);
    QVERIFY(before->isEmpty());

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    auto request = validSeriesEditRequest();
    request.repeatSeriesId = calendarEventSeriesId(
        repeatSeriesId.toUtf8().toStdString()
        );
    const auto edited = port.editRepeatSeriesFromDate(request);

    QVERIFY(edited);
    const auto after = repository->loadCalendarEventsForRepeatSeriesFromDate(
        repeatSeriesId,
        QDate(2026, 12, 8)
        );
    QVERIFY(after);
    QVERIFY(after->isEmpty());

    const auto unchanged = repository->getCalendarEvent(eventId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->title, QStringLiteral("Before selected suffix"));
    QCOMPARE(unchanged->startDate, QDate(2026, 12, 1));
    QCOMPARE(unchanged->endDate, QDate(2026, 12, 1));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsInvalidRepeatSeriesSourceDateBeforePersistence()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const QString repeatSeriesId = QStringLiteral("series-invalid-source-date");
    CalendarEvent event = makeEvent(
        QStringLiteral("Invalid source date"),
        QDate(2026, 12, 10),
        QDate(2026, 12, 10)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    event.repeatSeriesId = repeatSeriesId;
    const auto saved = repository->saveCalendarEvent(event);
    QVERIFY(saved);
    const int eventId = *saved;
    QVERIFY(eventId > 0);

    QSqlQuery corruptSourceDate(session->database());
    corruptSourceDate.prepare(QStringLiteral(
        "UPDATE calendar_events SET start_date=? WHERE id=?"
        ));
    corruptSourceDate.addBindValue(QStringLiteral("2026-12-40"));
    corruptSourceDate.addBindValue(eventId);
    QVERIFY(corruptSourceDate.exec());

    const auto selected = repository->loadCalendarEventsForRepeatSeriesFromDate(
        repeatSeriesId,
        QDate(2026, 12, 8)
        );
    QVERIFY(selected);
    QCOMPARE(selected->size(), 1);
    QVERIFY(!selected->front().startDate.isValid());

    QSqlQuery rejectPersistence(session->database());
    QVERIFY(rejectPersistence.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_series_edit "
        "BEFORE UPDATE ON calendar_events "
        "BEGIN SELECT RAISE(ABORT, 'series edit persistence was invoked'); END"
        )));

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    auto request = validSeriesEditRequest();
    request.repeatSeriesId = calendarEventSeriesId(
        repeatSeriesId.toUtf8().toStdString()
        );
    const auto edited = port.editRepeatSeriesFromDate(request);

    verifyFailure(edited, ErrorCode::Technical);
    QCOMPARE(
        edited.error().message,
        std::string(
            "A calendar repeat-series occurrence has an invalid start date."
            )
        );
    const auto unchanged = repository->getCalendarEvent(eventId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->title, QStringLiteral("Invalid source date"));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsRepeatSeriesDateOverflowBeforePersistence()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const QString repeatSeriesId = QStringLiteral("series-edit-date-overflow");
    CalendarEvent event = makeEvent(
        QStringLiteral("At supported date boundary"),
        QDate(9999, 12, 31),
        QDate(9999, 12, 31)
        );
    event.allDay = true;
    event.repeatSeriesId = repeatSeriesId;
    const auto saved = repository->saveCalendarEvent(event);
    QVERIFY(saved);
    const int eventId = *saved;
    QVERIFY(eventId > 0);

    QSqlQuery rejectPersistence(session->database());
    QVERIFY(rejectPersistence.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_series_edit "
        "BEFORE UPDATE ON calendar_events "
        "BEGIN SELECT RAISE(ABORT, 'series edit persistence was invoked'); END"
        )));

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    auto request = validSeriesEditRequest();
    request.repeatSeriesId = calendarEventSeriesId(
        repeatSeriesId.toUtf8().toStdString()
        );
    request.startDate = *GregorianDate::fromIsoString("9999-12-30");
    request.editedStartDate = *GregorianDate::fromIsoString("9999-12-30");
    request.editedEndDate = *GregorianDate::fromIsoString("9999-12-31");
    request.startTime.reset();
    request.endTime.reset();
    request.allDay = true;
    const auto edited = port.editRepeatSeriesFromDate(request);

    verifyFailure(edited, ErrorCode::Technical);
    QCOMPARE(
        edited.error().message,
        std::string(
            "A calendar repeat-series edit would move an occurrence outside the supported date range."
            )
        );
    const auto unchanged = repository->getCalendarEvent(eventId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->title, QStringLiteral("At supported date boundary"));
    QCOMPARE(unchanged->startDate, QDate(9999, 12, 31));
    QCOMPARE(unchanged->endDate, QDate(9999, 12, 31));
}

void NextPlatformApplicationServicesCalendarEventPortTests::
propagatesAllDayAndUnknownTimePolicy()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

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
        const auto saved = repository->saveCalendarEvent(event);
        QVERIFY(saved);
        const int eventId = *saved;
        QVERIFY(eventId > 0);
        eventIds.append(eventId);
    }

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    CalendarEventSeriesEditRequest allDayRequest{
        calendarEventSeriesId(repeatSeriesId.toUtf8().toStdString()),
        *GregorianDate::fromIsoString("2027-01-05"),
        *GregorianDate::fromIsoString("2027-01-06"),
        *GregorianDate::fromIsoString("2027-01-08"),
        "All-day policy",
        std::nullopt,
        std::nullopt,
        true,
        CalendarEventType::Holiday,
        CalendarEventTimeStatus::Timed
    };
    QVERIFY(port.editRepeatSeriesFromDate(allDayRequest));

    for (qsizetype index = 0; index < eventIds.size(); ++index)
    {
        const auto loaded = repository->getCalendarEvent(eventIds.at(index));
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
        calendarEventSeriesId(repeatSeriesId.toUtf8().toStdString()),
        *GregorianDate::fromIsoString("2027-01-12"),
        *GregorianDate::fromIsoString("2027-01-14"),
        *GregorianDate::fromIsoString("2027-01-15"),
        "Unknown policy",
        std::nullopt,
        std::nullopt,
        false,
        CalendarEventType::Vacation,
        CalendarEventTimeStatus::Unknown
    };
    QVERIFY(port.editRepeatSeriesFromDate(unknownRequest));

    const auto untouched = repository->getCalendarEvent(eventIds.at(0));
    QVERIFY(untouched);
    QVERIFY(untouched->allDay);
    QCOMPARE(untouched->title, QStringLiteral("All-day policy"));

    for (qsizetype index = 1; index < eventIds.size(); ++index)
    {
        const auto loaded = repository->getCalendarEvent(eventIds.at(index));
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
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    auto invalid = validSeriesEditRequest();
    invalid.repeatSeriesId = calendarEventSeriesId(" \t");
    verifyFailure(
        port.editRepeatSeriesFromDate(invalid),
        ErrorCode::InvalidInput
        );

    invalid = validSeriesEditRequest();
    invalid.repeatSeriesId = CalendarEventSeriesId::fromString("");
    verifyFailure(
        port.editRepeatSeriesFromDate(invalid),
        ErrorCode::InvalidInput
        );

    const std::vector<std::string> unicodeWhitespaceIds{
        "\xC2\xA0",
        "\xE3\x80\x80"
    };
    for (const std::string& repeatSeriesId : unicodeWhitespaceIds)
    {
        invalid = validSeriesEditRequest();
        invalid.repeatSeriesId = calendarEventSeriesId(repeatSeriesId);
        const auto result = port.editRepeatSeriesFromDate(invalid);
        verifyFailure(result, ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string(
                "Calendar repeat-series identifier must be non-blank and "
                "bounded."
                )
            );
    }

    invalid = validSeriesEditRequest();
    invalid.editedStartDate = *GregorianDate::fromIsoString("2026-12-11");
    invalid.editedEndDate = *GregorianDate::fromIsoString("2026-12-09");
    const auto reversedRange = port.editRepeatSeriesFromDate(invalid);
    verifyFailure(reversedRange, ErrorCode::InvalidInput);
    QCOMPARE(
        reversedRange.error().message,
        std::string(
            "Calendar repeat-series edit end date must not precede its start date."
            )
        );

    invalid = validSeriesEditRequest();
    invalid.title = " \t ";
    const auto invalidText = port.editRepeatSeriesFromDate(invalid);
    verifyFailure(invalidText, ErrorCode::InvalidInput);
    QCOMPARE(
        invalidText.error().message,
        std::string(
            "Calendar repeat-series edit text fields must be non-blank and bounded."
            )
        );

    invalid = validSeriesEditRequest();
    invalid.eventType = static_cast<CalendarEventType>(99);
    const auto invalidEventType = port.editRepeatSeriesFromDate(invalid);
    verifyFailure(invalidEventType, ErrorCode::InvalidInput);
    QCOMPARE(
        invalidEventType.error().message,
        std::string(
            "Calendar repeat-series edit text fields must be non-blank and bounded."
            )
        );

    invalid = validSeriesEditRequest();
    invalid.timeStatus = static_cast<CalendarEventTimeStatus>(99);
    invalid.startTime.reset();
    invalid.endTime.reset();
    const auto invalidTimeStatus = port.editRepeatSeriesFromDate(invalid);
    verifyFailure(invalidTimeStatus, ErrorCode::InvalidInput);
    QCOMPARE(
        invalidTimeStatus.error().message,
        std::string(
            "Calendar repeat-series edit time status must be Timed, Unknown, or Unconfirmed."
            )
        );

    invalid = validSeriesEditRequest();
    invalid.allDay = true;
    verifyFailure(
        port.editRepeatSeriesFromDate(invalid),
        ErrorCode::InvalidInput
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableRepeatSeriesEditSessionStructurally()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());
    ApplicationServicesCalendarEventSeriesEditPort port(services);

    auto invalid = validSeriesEditRequest();
    invalid.repeatSeriesId = calendarEventSeriesId(" \t");
    verifyFailure(
        port.editRepeatSeriesFromDate(invalid),
        ErrorCode::InvalidInput
        );

    const auto unavailable = port.editRepeatSeriesFromDate(
        validSeriesEditRequest()
        );
    verifyRecoverableFailure(
        unavailable,
        ErrorCode::NotFound,
        "The calendar event repository is unavailable."
        );

    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(session->isOpen());
    QVERIFY(session->calendarEventRepository());
    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    const auto closed = port.editRepeatSeriesFromDate(
        validSeriesEditRequest()
        );
    verifyRecoverableFailure(
        closed,
        ErrorCode::NotFound,
        "The calendar event repository is unavailable."
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsRepeatSeriesEditReadFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->calendarEventRepository());
    QSqlQuery query(session->database());
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
reportsRepeatSeriesEditTerminalReadFailureBeforePersistence()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    QVERIFY(session->calendarEventRepository());
    const QSqlDatabase database = session->database();

    QSqlQuery setup(database);
    QVERIFY2(
        setup.exec(QStringLiteral("PRAGMA foreign_keys = OFF")),
        qPrintable(setup.lastError().text())
        );
    QVERIFY2(
        setup.exec(QStringLiteral("DROP TABLE calendar_events")),
        qPrintable(setup.lastError().text())
        );
    QVERIFY2(setup.exec(QStringLiteral(R"(
        CREATE TABLE calendar_events_f928_backing (
            id INTEGER PRIMARY KEY,
            repeat_series_id TEXT NOT NULL,
            start_date TEXT NOT NULL,
            start_time TEXT NOT NULL,
            title TEXT NOT NULL,
            payload TEXT NOT NULL
        )
    )")), qPrintable(setup.lastError().text()));
    QVERIFY2(setup.exec(QStringLiteral(R"(
        CREATE INDEX calendar_events_f928_scan
        ON calendar_events_f928_backing (
            repeat_series_id,
            start_date,
            start_time,
            title,
            id,
            payload
        )
    )")), qPrintable(setup.lastError().text()));

    QSqlQuery insert(database);
    QVERIFY2(insert.prepare(QStringLiteral(R"(
        INSERT INTO calendar_events_f928_backing (
            id,
            repeat_series_id,
            start_date,
            start_time,
            title,
            payload
        ) VALUES (?, ?, ?, ?, ?, ?)
    )")), qPrintable(insert.lastError().text()));
    insert.bindValue(0, 1);
    insert.bindValue(1, QStringLiteral("series-edit"));
    insert.bindValue(2, QStringLiteral("2026-12-08"));
    insert.bindValue(3, QStringLiteral("09:00"));
    insert.bindValue(4, QStringLiteral("Original valid occurrence"));
    insert.bindValue(5, QStringLiteral(
        R"({"event_type":"Meeting","time_status":"Timed","all_day":0,"end_date":"2026-12-08","end_time":"10:00"})"
        ));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    insert.bindValue(0, 2);
    insert.bindValue(1, QStringLiteral("series-edit"));
    insert.bindValue(2, QStringLiteral("2026-12-09"));
    insert.bindValue(3, QStringLiteral("09:00"));
    insert.bindValue(4, QStringLiteral("Later malformed occurrence"));
    insert.bindValue(5, QStringLiteral("{"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    QVERIFY2(setup.exec(QStringLiteral(R"(
        CREATE VIEW calendar_events AS
        SELECT
            id,
            title,
            json_extract(payload, '$.event_type') AS event_type,
            json_extract(payload, '$.time_status') AS time_status,
            repeat_series_id,
            json_extract(payload, '$.all_day') AS all_day,
            start_date,
            start_time,
            json_extract(payload, '$.end_date') AS end_date,
            json_extract(payload, '$.end_time') AS end_time
        FROM calendar_events_f928_backing
        INDEXED BY calendar_events_f928_scan
    )")), qPrintable(setup.lastError().text()));
    QVERIFY2(setup.exec(QStringLiteral(R"(
        CREATE TRIGGER calendar_events_f928_reject_update
        INSTEAD OF UPDATE ON calendar_events
        BEGIN
            SELECT RAISE(ABORT, 'F928_PERSISTENCE_REACHED');
        END
    )")), qPrintable(setup.lastError().text()));

    QSqlQuery probe(database);
    QVERIFY2(probe.prepare(QStringLiteral(R"(
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
        WHERE repeat_series_id=?
        AND start_date >= ?
        ORDER BY start_date, start_time, title, id
    )")), qPrintable(probe.lastError().text()));
    probe.addBindValue(QStringLiteral("series-edit"));
    probe.addBindValue(QStringLiteral("2026-12-08"));
    QVERIFY2(probe.exec(), qPrintable(probe.lastError().text()));
    QVERIFY2(probe.next(), qPrintable(probe.lastError().text()));
    QCOMPARE(probe.value(QStringLiteral("id")).toLongLong(), qint64(1));
    QCOMPARE(
        probe.value(QStringLiteral("title")).toString(),
        QStringLiteral("Original valid occurrence")
        );
    QCOMPARE(
        probe.value(QStringLiteral("event_type")).toString(),
        QStringLiteral("Meeting")
        );
    QVERIFY(!probe.next());
    QVERIFY2(
        probe.lastError().isValid(),
        "The second production-shaped fetch must fail at the malformed JSON row."
        );
    QVERIFY2(
        probe.lastError().text().contains(
            QStringLiteral("malformed JSON"),
            Qt::CaseInsensitive
            ),
        qPrintable(probe.lastError().text())
        );

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    const auto edited = port.editRepeatSeriesFromDate(
        validSeriesEditRequest()
        );

    verifyFailure(edited, ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(edited.error().message);
    QVERIFY2(
        diagnostic.contains(
            QStringLiteral("Loading calendar repeat series events"),
            Qt::CaseInsensitive
            ),
        qPrintable(diagnostic)
        );
    QVERIFY2(
        !diagnostic.contains(QStringLiteral("F928_PERSISTENCE_REACHED")),
        qPrintable(diagnostic)
        );

    QSqlQuery count(database);
    QVERIFY2(
        count.exec(QStringLiteral(
            "SELECT COUNT(*) FROM calendar_events_f928_backing"
            )),
        qPrintable(count.lastError().text())
        );
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 2);

    QSqlQuery unchanged(database);
    QVERIFY2(unchanged.exec(QStringLiteral(R"(
        SELECT
            id,
            repeat_series_id,
            start_date,
            start_time,
            title,
            payload
        FROM calendar_events_f928_backing
        WHERE id = 1
    )")), qPrintable(unchanged.lastError().text()));
    QVERIFY(unchanged.next());
    QCOMPARE(unchanged.value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(
        unchanged.value(QStringLiteral("repeat_series_id")).toString(),
        QStringLiteral("series-edit")
        );
    QCOMPARE(
        unchanged.value(QStringLiteral("start_date")).toString(),
        QStringLiteral("2026-12-08")
        );
    QCOMPARE(
        unchanged.value(QStringLiteral("start_time")).toString(),
        QStringLiteral("09:00")
        );
    QCOMPARE(
        unchanged.value(QStringLiteral("title")).toString(),
        QStringLiteral("Original valid occurrence")
        );
    QCOMPARE(
        unchanged.value(QStringLiteral("payload")).toString(),
        QStringLiteral(
            R"({"event_type":"Meeting","time_status":"Timed","all_day":0,"end_date":"2026-12-08","end_time":"10:00"})"
            )
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsRepeatSeriesEditSaveFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const QString repeatSeriesId = QStringLiteral("series-edit-failure");
    const QList<QDate> occurrenceDates{
        QDate(2026, 12, 8),
        QDate(2026, 12, 15)
    };
    QList<int> eventIds;
    for (const QDate& startDate : occurrenceDates)
    {
        CalendarEvent event = makeEvent(
            QStringLiteral("Persisted before failure"),
            startDate,
            startDate
            );
        event.startTime = QTime(9, 0);
        event.endTime = QTime(10, 0);
        event.repeatSeriesId = repeatSeriesId;
        const auto saved = repository->saveCalendarEvent(event);
        QVERIFY(saved);
        const int eventId = *saved;
        QVERIFY(eventId > 0);
        eventIds.append(eventId);
    }

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_repeat_edit "
        "BEFORE UPDATE ON calendar_events "
        "WHEN NEW.start_date = '2026-12-17' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar repeat edit failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventSeriesEditPort port(services);
    auto request = validSeriesEditRequest();
    request.repeatSeriesId = calendarEventSeriesId(
        repeatSeriesId.toUtf8().toStdString()
        );
    const auto edited = port.editRepeatSeriesFromDate(request);

    verifyFailure(edited, ErrorCode::Technical);
    QVERIFY(
        edited.error().message.find("Updating calendar event")
            != std::string::npos
        || edited.error().message.find("injected calendar repeat edit")
            != std::string::npos
        );

    for (qsizetype index = 0; index < eventIds.size(); ++index)
    {
        const auto loaded = repository->getCalendarEvent(eventIds.at(index));
        QVERIFY(loaded);
        QCOMPARE(loaded->title, QStringLiteral("Persisted before failure"));
        QCOMPARE(loaded->repeatSeriesId, repeatSeriesId);
        QCOMPARE(loaded->startDate, occurrenceDates.at(index));
        QCOMPARE(loaded->endDate, occurrenceDates.at(index));
        QCOMPARE(loaded->startTime, QTime(9, 0));
        QCOMPARE(loaded->endTime, QTime(10, 0));
    }
}

void NextPlatformApplicationServicesCalendarEventPortTests::
deletesValidRepeatSeriesSuffix()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const QString repeatSeriesId = QStringLiteral("series-delete-suffix");
    QList<int> eventIds;
    for (const QDate& startDate : {
             QDate(2024, 2, 28),
             QDate(2024, 2, 29),
             QDate(2024, 3, 1)
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
        const auto saved = repository->saveCalendarEvent(event);
        QVERIFY(saved);
        const int eventId = *saved;
        QVERIFY(eventId > 0);
        eventIds.append(eventId);
    }

    CalendarEvent unrelatedSeriesEvent = makeEvent(
        QStringLiteral("Unrelated series event"),
        QDate(2024, 3, 1),
        QDate(2024, 3, 1)
        );
    unrelatedSeriesEvent.startTime = QTime(13, 0);
    unrelatedSeriesEvent.endTime = QTime(14, 0);
    unrelatedSeriesEvent.repeatSeriesId = QStringLiteral("series-delete-other");
    const auto unrelatedSeriesSaved =
        repository->saveCalendarEvent(unrelatedSeriesEvent);
    QVERIFY(unrelatedSeriesSaved);
    QVERIFY(*unrelatedSeriesSaved > 0);

    CalendarEvent unrelatedEvent = makeEvent(
        QStringLiteral("Unrelated one-time event"),
        QDate(2024, 2, 29),
        QDate(2024, 2, 29)
        );
    unrelatedEvent.startTime = QTime(15, 0);
    unrelatedEvent.endTime = QTime(16, 0);
    const auto unrelatedSaved = repository->saveCalendarEvent(unrelatedEvent);
    QVERIFY(unrelatedSaved);
    QVERIFY(*unrelatedSaved > 0);

    for (const int eventId : eventIds)
    {
        QVERIFY(repository->getCalendarEvent(eventId));
    }
    QVERIFY(repository->getCalendarEvent(*unrelatedSeriesSaved));
    QVERIFY(repository->getCalendarEvent(*unrelatedSaved));

    ApplicationServicesCalendarEventSeriesDeletePort port(services);
    const CalendarEventSeriesDeleteRequest request{
        calendarEventSeriesId(
            QStringLiteral("  series-delete-suffix  ").toUtf8().toStdString()
            ),
        gregorianDate("2024-02-29")
    };
    const auto deleted = port.deleteRepeatSeriesFromDate(request);

    QVERIFY(deleted);
    QVERIFY(repository->getCalendarEvent(eventIds.at(0)));
    QVERIFY(!repository->getCalendarEvent(eventIds.at(1)));
    QVERIFY(!repository->getCalendarEvent(eventIds.at(2)));
    QVERIFY(repository->getCalendarEvent(*unrelatedSeriesSaved));
    QVERIFY(repository->getCalendarEvent(*unrelatedSaved));

    const auto remaining = repository->loadCalendarEventsInRange(
        QDate(2024, 2, 1),
        QDate(2024, 3, 31)
        );
    QVERIFY(remaining);
    QCOMPARE(remaining->size(), 3);
}

void NextPlatformApplicationServicesCalendarEventPortTests::
deletesMissingRepeatSeriesSuffixSuccessfully()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    const QString repeatSeriesId = QStringLiteral("series-delete-missing");
    const auto before = repository->loadCalendarEventsForRepeatSeriesFromDate(
        repeatSeriesId,
        QDate(2026, 12, 8)
        );
    QVERIFY(before);
    QVERIFY(before->isEmpty());

    ApplicationServicesCalendarEventSeriesDeletePort port(services);
    const auto deleted = port.deleteRepeatSeriesFromDate({
        calendarEventSeriesId(repeatSeriesId.toUtf8().toStdString()),
        gregorianDate("2026-12-08")
    });

    QVERIFY(deleted);
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsInvalidRepeatSeriesDeleteRequestStructurally()
{
    const std::string diagnostic =
        "Calendar repeat-series delete request must contain a non-blank "
        "bounded series identifier and a valid ISO start date.";
    ApplicationServices unavailableServices;
    ApplicationServicesCalendarEventSeriesDeletePort unavailablePort(
        unavailableServices
        );
    const auto invalidBeforeServiceLookup =
        unavailablePort.deleteRepeatSeriesFromDate({
            calendarEventSeriesId("   "),
            gregorianDate("2026-12-08")
        });
    verifyFailure(invalidBeforeServiceLookup, ErrorCode::InvalidInput);
    QCOMPARE(invalidBeforeServiceLookup.error().message, diagnostic);
    const auto emptyId = CalendarEventSeriesId::fromString("");
    QVERIFY(!emptyId);
    const auto absentBeforeServiceLookup =
        unavailablePort.deleteRepeatSeriesFromDate({
            emptyId,
            gregorianDate("2026-12-08")
        });
    verifyFailure(absentBeforeServiceLookup, ErrorCode::InvalidInput);
    QCOMPARE(absentBeforeServiceLookup.error().message, diagnostic);

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesCalendarEventSeriesDeletePort port(services);
    const auto blankSeriesId = port.deleteRepeatSeriesFromDate({
        calendarEventSeriesId("   "),
        gregorianDate("2026-12-08")
    });
    verifyFailure(blankSeriesId, ErrorCode::InvalidInput);
    QCOMPARE(blankSeriesId.error().message, diagnostic);
    verifyFailure(
        port.deleteRepeatSeriesFromDate({
            calendarEventSeriesId(std::string(
                kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength + 1,
                'r'
                )),
            gregorianDate("2026-12-08")
        }),
        ErrorCode::InvalidInput
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableRepeatSeriesDeleteSessionStructurally()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(!session->isOpen());
    ApplicationServicesCalendarEventSeriesDeletePort port(services);

    verifyRecoverableFailure(
        port.deleteRepeatSeriesFromDate({
            calendarEventSeriesId("series-delete-unavailable"),
            gregorianDate("2026-12-08")
        }),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );

    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(session->isOpen());
    QVERIFY(session->calendarEventRepository());

    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());
    verifyRecoverableFailure(
        port.deleteRepeatSeriesFromDate({
            calendarEventSeriesId("series-delete-unavailable"),
            gregorianDate("2026-12-08")
        }),
        ErrorCode::NotFound,
        "The calendar service is unavailable."
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsRepeatSeriesDeleteServiceFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    CalendarEventRepository* const repository =
        session->calendarEventRepository();
    QVERIFY(repository);

    CalendarEvent event = makeEvent(
        QStringLiteral("Repeat delete failure"),
        QDate(2026, 12, 2),
        QDate(2026, 12, 2)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    event.repeatSeriesId = QStringLiteral("series-delete-failure");
    const auto saved = repository->saveCalendarEvent(event);
    QVERIFY(saved);
    const int eventId = *saved;
    QVERIFY(eventId > 0);

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_calendar_repeat_delete "
        "BEFORE DELETE ON calendar_events "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected calendar repeat delete failure'); "
        "END"
        )));

    ApplicationServicesCalendarEventSeriesDeletePort port(services);
    const auto deleted = port.deleteRepeatSeriesFromDate({
        calendarEventSeriesId("series-delete-failure"),
        gregorianDate("2026-12-02")
    });

    verifyFailure(deleted, ErrorCode::Technical);
    QVERIFY(
        deleted.error().message.find("Deleting calendar repeat series")
            != std::string::npos
        );
    QVERIFY(repository->getCalendarEvent(eventId));
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

    QSqlQuery legacyAllDayInsert(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY2(
        legacyAllDayInsert.exec(QStringLiteral(
            "INSERT INTO calendar_events ("
            "title, event_type, time_status, repeat_series_id, all_day, "
            "start_date, start_time, end_date, end_time) "
            "VALUES ('All-day unconfirmed legacy row', 'Other', "
            "'Unconfirmed', NULL, 1, '2026-09-20', NULL, "
            "'2026-09-21', NULL)"
            )),
        qPrintable(legacyAllDayInsert.lastError().text())
        );
    const int allDayUnconfirmedId = legacyAllDayInsert.lastInsertId().toInt();
    QVERIFY(allDayUnconfirmedId > 0);

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
    QCOMPARE(allDayProjection->eventType.value(), std::string("Vacation"));
    QCOMPARE(allDayProjection->timeStatus, CalendarEventTimeStatus::Timed);
    QCOMPARE(
        allDayProjection->repeatSeriesId->value(),
        std::string("series-all-day")
        );

    const auto unknownProjection = result.value().findEvent(
        calendarEventId(unknownTimeId)
        );
    QVERIFY(unknownProjection.has_value());
    QVERIFY(!unknownProjection->allDay);
    QVERIFY(!unknownProjection->startTime.has_value());
    QVERIFY(!unknownProjection->endTime.has_value());
    QCOMPARE(unknownProjection->eventType.value(), std::string("Holiday"));
    QCOMPARE(unknownProjection->timeStatus, CalendarEventTimeStatus::Unknown);
    QVERIFY(!unknownProjection->repeatSeriesId.has_value());

    const auto allDayById = port.projectionById(allDayId);
    QVERIFY(allDayById);
    QCOMPARE(allDayById.value().id.value(), std::to_string(allDayId));
    QCOMPARE(allDayById.value().startDate, std::string("2026-09-20"));
    QCOMPARE(allDayById.value().endDate, std::string("2026-09-21"));
    QVERIFY(allDayById.value().allDay);
    QVERIFY(!allDayById.value().startTime.has_value());
    QVERIFY(!allDayById.value().endTime.has_value());
    QCOMPARE(allDayById.value().eventType.value(), std::string("Vacation"));
    QCOMPARE(allDayById.value().timeStatus, CalendarEventTimeStatus::Timed);
    QCOMPARE(
        allDayById.value().repeatSeriesId,
        std::optional<CalendarEventSeriesId>(
            calendarEventSeriesId("series-all-day")
            )
        );

    const auto unknownById = port.projectionById(unknownTimeId);
    QVERIFY(unknownById);
    QVERIFY(!unknownById.value().allDay);
    QVERIFY(!unknownById.value().startTime.has_value());
    QVERIFY(!unknownById.value().endTime.has_value());
    QCOMPARE(unknownById.value().eventType.value(), std::string("Holiday"));
    QCOMPARE(unknownById.value().timeStatus, CalendarEventTimeStatus::Unknown);
    QVERIFY(!unknownById.value().repeatSeriesId.has_value());

    const auto legacyAllDayUnknown = result.value().findEvent(
        calendarEventId(allDayUnconfirmedId)
        );
    QVERIFY(legacyAllDayUnknown.has_value());
    QVERIFY(legacyAllDayUnknown->allDay);
    QVERIFY(!legacyAllDayUnknown->startTime.has_value());
    QVERIFY(!legacyAllDayUnknown->endTime.has_value());
    QCOMPARE(
        legacyAllDayUnknown->timeStatus,
        CalendarEventTimeStatus::Unconfirmed
        );
    const auto legacyAllDayUnknownById = port.projectionById(
        allDayUnconfirmedId
        );
    QVERIFY(legacyAllDayUnknownById);
    QVERIFY(legacyAllDayUnknownById.value().allDay);
    QCOMPARE(
        legacyAllDayUnknownById.value().timeStatus,
        CalendarEventTimeStatus::Unconfirmed
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableAndInvalidRangesStructurally()
{
    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    ApplicationServicesCalendarEventPort unavailablePort(
        unavailableServices
        );
    QVERIFY(!unavailablePort.isAvailable());
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
    verifyFailure(
        unavailablePort.loadEventById(calendarEventId(1)),
        ErrorCode::NotFound
        );

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesCalendarEventPort port(services);
    QVERIFY(port.isAvailable());
    verifyFailure(
        port.projectionById(0),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.loadEventById(calendarEventId(0)),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.loadEventById(*CalendarEventId::fromString("not-a-number")),
        ErrorCode::InvalidInput
        );
    CalendarEventRepository* const repository =
        services.databaseSession()->calendarEventRepository();
    QVERIFY(repository);
    const int missingId = 999999;
    const CalendarEventByIdLookupResult missingLookup =
        repository->lookupCalendarEventById(missingId);
    QCOMPARE(missingLookup.code, CalendarEventByIdLookupCode::Missing);
    const QString missingDiagnostic = QObject::tr(
        "Loading calendar event failed: no matching record exists for event id %1."
        ).arg(missingId);
    QCOMPARE(missingLookup.diagnostic, missingDiagnostic);
    const ::Result<CalendarEvent> legacyMissing =
        repository->getCalendarEvent(missingId);
    QVERIFY(!legacyMissing);
    QCOMPARE(legacyMissing.error(), missingDiagnostic);

    const auto missing = port.projectionById(missingId);
    verifyFailure(missing, ErrorCode::NotFound);
    QCOMPARE(
        missing.error().message,
        std::string("The calendar event was not found.")
        );
    verifyFailure(
        port.projection(QDate(), QDate(2026, 9, 21)),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.projection(QDate(2026, 9, 21), QDate(2026, 9, 20)),
        ErrorCode::InvalidInput
        );

    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!port.isAvailable());
    verifyFailure(
        port.projection(QDate(2026, 9, 20), QDate(2026, 9, 21)),
        ErrorCode::NotFound
        );
    verifyFailure(port.projectionById(1), ErrorCode::NotFound);
    verifyFailure(
        port.loadEventById(calendarEventId(1)),
        ErrorCode::NotFound
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsCalendarEventRepositoryReadFailuresStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* const service = services.calendarService();
    QVERIFY(service);
    CalendarEvent event = makeEvent(
        QStringLiteral("Read failure"),
        QDate(2026, 9, 20),
        QDate(2026, 9, 21)
        );
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = saveEvent(
        *service,
        event
        );
    QVERIFY(eventId > 0);

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(
        query.exec(QStringLiteral("DROP TABLE calendar_events")),
        qPrintable(query.lastError().text())
        );

    CalendarEventRepository* const repository =
        services.databaseSession()->calendarEventRepository();
    QVERIFY(repository);
    const CalendarEventByIdLookupResult failedLookup =
        repository->lookupCalendarEventById(eventId);
    QCOMPARE(failedLookup.code, CalendarEventByIdLookupCode::Failure);
    QVERIFY(!failedLookup.diagnostic.isEmpty());

    ApplicationServicesCalendarEventPort port(services);
    verifyFailure(
        port.projection(QDate(2026, 9, 20), QDate(2026, 9, 21)),
        ErrorCode::Technical
        );
    const auto byIdFailure = port.projectionById(eventId);
    verifyFailure(byIdFailure, ErrorCode::Technical);
    const QByteArray repositoryErrorBytes = failedLookup.diagnostic.toUtf8();
    QVERIFY(!repositoryErrorBytes.isEmpty());
    QCOMPARE(
        byIdFailure.error().message,
        "Calendar event could not be loaded: "
            + repositoryErrorBytes.toStdString()
        );
    verifyFailure(
        port.loadEventById(calendarEventId(eventId)),
        ErrorCode::Technical
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
returnsLegacyImportSignatureKeysInRangeOrder()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    CalendarService* legacyService = services.calendarService();
    QVERIFY(legacyService);

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY2(
        query.prepare(QStringLiteral(
            "INSERT INTO calendar_events ("
            "title, event_type, time_status, repeat_series_id, all_day, "
            "start_date, start_time, end_date, end_time) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
            )),
        qPrintable(query.lastError().text())
        );

    const auto insertEvent = [&query](
        const QString& title,
        const QString& eventType,
        const QString& timeStatus,
        const QString& repeatSeriesId,
        const int allDay,
        const QString& startDate,
        const QString& startTime,
        const QString& endDate,
        const QString& endTime
        )
    {
        query.bindValue(0, title);
        query.bindValue(1, eventType);
        query.bindValue(2, timeStatus);
        query.bindValue(3, repeatSeriesId);
        query.bindValue(4, allDay);
        query.bindValue(5, startDate);
        query.bindValue(6, startTime);
        query.bindValue(7, endDate);
        query.bindValue(8, endTime);
        return query.exec();
    };

    const QString firstStartDate = QStringLiteral("2026-09-23");
    const QString firstEndDate = QStringLiteral("2026-09-24");
    const QString duplicateTitle =
        QStringLiteral("  Caf\u00E9   \u5348\u524D  ");
    const QString canonicalDuplicateTitle =
        QStringLiteral("Caf\u00E9 \u5348\u524D");
    QVERIFY2(
        insertEvent(
            duplicateTitle,
            QStringLiteral("Meeting"),
            QStringLiteral("Unconfirmed"),
            QStringLiteral("series-first"),
            0,
            firstStartDate,
            QStringLiteral("09:00"),
            firstEndDate,
            QStringLiteral("10:00")
            ),
        qPrintable(query.lastError().text())
        );
    QVERIFY2(
        insertEvent(
            QStringLiteral("  \u6771\u4EAC   \u6708\u4F8B\u4F1A  "),
            QStringLiteral("Other"),
            QStringLiteral("Timed"),
            QStringLiteral("series-middle"),
            0,
            firstStartDate,
            QStringLiteral("10:00"),
            firstStartDate,
            QStringLiteral("10:30")
            ),
        qPrintable(query.lastError().text())
        );
    QVERIFY2(
        insertEvent(
            canonicalDuplicateTitle,
            QStringLiteral("Meeting"),
            QStringLiteral("Unconfirmed"),
            QStringLiteral("series-second"),
            0,
            firstStartDate,
            QStringLiteral("11:00"),
            firstEndDate,
            QStringLiteral("12:00")
            ),
        qPrintable(query.lastError().text())
        );

    const QDate startDate(2026, 9, 23);
    const QDate endDate(2026, 9, 24);
    const auto legacyRows = legacyService->eventsInRange(startDate, endDate);
    QVERIFY(legacyRows);
    QCOMPARE(legacyRows->size(), static_cast<qsizetype>(3));
    QVERIFY(legacyRows->at(0).id != legacyRows->at(2).id);
    QVERIFY(legacyRows->at(0).startTime != legacyRows->at(2).startTime);
    QVERIFY(
        legacyRows->at(0).repeatSeriesId
        != legacyRows->at(2).repeatSeriesId
        );

    ApplicationServicesCalendarEventImportSignatureQueryPort port(services);
    const auto keys = port.loadSignaturesInRange(
        importSignatureRange(startDate, endDate)
        );
    QVERIFY(keys);
    const auto& keyValues = keys.value();
    QCOMPARE(keyValues.size(), std::size_t{3});

    const QString expectedDuplicateKey = QStringLiteral(
        "Caf\u00E9 \u5348\u524D|Meeting|2026-09-23|2026-09-24|0|Unconfirmed"
        );
    const QString expectedOtherKey = QStringLiteral(
        "\u6771\u4EAC \u6708\u4F8B\u4F1A|Other|2026-09-23|2026-09-23|0|Timed"
        );
    const std::vector<QString> expectedKeys = {
        expectedDuplicateKey,
        expectedOtherKey,
        expectedDuplicateKey
    };
    static_assert(std::is_same_v<
        CalendarEventImportSignatureKeys,
        std::vector<CalendarEventImportSignature>
        >);
    for (std::size_t index = 0; index < expectedKeys.size(); ++index)
    {
        const QString actual =
            QString::fromStdU16String(keyValues.at(index).value());
        QCOMPARE(actual, expectedKeys.at(index));
        QVERIFY(
            keyValues.at(index)
            ==
            CalendarImport::calendarEventImportSignature(
                legacyRows->at(static_cast<qsizetype>(index))
                )
            );
    }
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsUnavailableAndInvalidImportSignatureRangesStructurally()
{
    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    ApplicationServicesCalendarEventImportSignatureQueryPort unavailablePort(
        unavailableServices
        );
    QVERIFY(!unavailablePort.isAvailable());
    verifyRecoverableFailure(
        unavailablePort.loadSignaturesInRange(importSignatureRange(
            QDate(2026, 9, 20),
            QDate(2026, 9, 21)
            )),
        ErrorCode::NotFound,
        "The calendar event repository is unavailable."
        );

    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    ApplicationServicesCalendarEventImportSignatureQueryPort port(services);
    QVERIFY(port.isAvailable());
    verifyFailure(
        port.loadSignaturesInRange(importSignatureRange(
            QDate(),
            QDate(2026, 9, 21)
            )),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.loadSignaturesInRange(importSignatureRange(
            QDate(2026, 9, 21),
            QDate(2026, 9, 20)
            )),
        ErrorCode::InvalidInput
        );
    verifyFailure(
        port.loadSignaturesInRange({
            CalendarEventDate("2026-9-21"),
            CalendarEventDate("2026-09-22")
        }),
        ErrorCode::InvalidInput
        );

    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!port.isAvailable());
    verifyRecoverableFailure(
        port.loadSignaturesInRange(importSignatureRange(
            QDate(2026, 9, 20),
            QDate(2026, 9, 21)
            )),
        ErrorCode::NotFound,
        "The calendar event repository is unavailable."
        );
}

void NextPlatformApplicationServicesCalendarEventPortTests::
reportsImportSignatureRangeReadFailureStructurally()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    QSqlQuery query(
        services.dataService()->databaseSession()->database()
        );
    QVERIFY2(
        query.exec(QStringLiteral("DROP TABLE calendar_events")),
        qPrintable(query.lastError().text())
        );

    ApplicationServicesCalendarEventImportSignatureQueryPort port(services);
    verifyFailure(
        port.loadSignaturesInRange(importSignatureRange(
            QDate(2026, 9, 20),
            QDate(2026, 9, 21)
            )),
        ErrorCode::Technical
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
                static_cast<qulonglong>(index),
                4,
                10,
                QLatin1Char('0')
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

    // Make the final ordered source row invalid for projection. The raw-row
    // capacity boundary must win before the adapter converts that row.
    QSqlQuery malformedOverflowRow(
        services.dataService()->databaseSession()->database()
        );
    malformedOverflowRow.prepare(QStringLiteral(
        "UPDATE calendar_events SET title=? WHERE id=?"
        ));
    malformedOverflowRow.addBindValue(QString(
        static_cast<int>(kCalendarEventSummaryMaxTitleUtf8Bytes + 1),
        QChar('z')
        ));
    malformedOverflowRow.addBindValue(saved->back());
    QVERIFY(malformedOverflowRow.exec());
    QCOMPARE(malformedOverflowRow.numRowsAffected(), 1);

    const auto overflow = port.projection(
        QDate(2026, 11, 1),
        QDate(2026, 11, 1)
        );
    verifyFailure(overflow, ErrorCode::InvalidInput);
    QCOMPARE(
        QString::fromStdString(overflow.error().message),
        QStringLiteral(
            "Calendar event range exceeds its bounded projection capacity."
            )
        );

    QSqlQuery restoreOverflowRow(
        services.dataService()->databaseSession()->database()
        );
    restoreOverflowRow.prepare(QStringLiteral(
        "UPDATE calendar_events SET title=? WHERE id=?"
        ));
    restoreOverflowRow.addBindValue(QStringLiteral("Overflow 4096"));
    restoreOverflowRow.addBindValue(saved->back());
    QVERIFY(restoreOverflowRow.exec());
    QCOMPARE(restoreOverflowRow.numRowsAffected(), 1);

    ApplicationServicesCalendarEventImportSignatureQueryPort importPort(
        services
        );
    const auto importKeys = importPort.loadSignaturesInRange(
        importSignatureRange(QDate(2026, 11, 1), QDate(2026, 11, 1))
        );
    QVERIFY(importKeys);
    const auto& importKeyValues = importKeys.value();
    QCOMPARE(
        importKeyValues.size(),
        kCalendarEventProjectionMaxEvents + 1
        );
    QCOMPARE(
        QString::fromStdU16String(importKeyValues.front().value()),
        QStringLiteral("Overflow 0000|Other|2026-11-01|2026-11-01|0|Timed")
        );
    QCOMPARE(
        QString::fromStdU16String(importKeyValues.back().value()),
        QStringLiteral("Overflow 4096|Other|2026-11-01|2026-11-01|0|Timed")
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
    using ImportSignatureQueryPort =
        CalendarEventImportSignatureQueryPort;
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
    using ByIdQueryResult = decltype(
        std::declval<Port&>().loadEventById(
            std::declval<const CalendarEventId&>()
            )
        );
    using ImportSignatureQueryResult = decltype(
        std::declval<const ImportSignatureQueryPort&>()
            .loadSignaturesInRange(
                std::declval<const CalendarEventImportSignatureRangeRequest&>()
                )
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
        ByIdQueryResult,
        CalendarEventByIdQueryResult
        >);
    static_assert(std::is_base_of_v<CalendarEventByIdQueryPort, Port>);
    static_assert(std::is_same_v<
        ImportSignatureQueryResult,
        CalendarEventImportSignatureQueryResult
        >);
    static_assert(std::is_base_of_v<
        ImportSignatureQueryPort,
        ApplicationServicesCalendarEventImportSignatureQueryPort
        >);
    static_assert(!std::is_copy_constructible_v<
        ApplicationServicesCalendarEventImportSignatureQueryPort
        >);
    static_assert(!std::is_move_constructible_v<
        ApplicationServicesCalendarEventImportSignatureQueryPort
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
        decltype(std::declval<CalendarEventSaveRequest>().startDate),
        GregorianDate
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSaveRequest>().endDate),
        GregorianDate
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
        std::optional<CalendarEventSeriesId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesDeleteRequest>().startDate),
        GregorianDate
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
        std::optional<CalendarEventSeriesId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesCreateRequest>()
                     .occurrences),
        std::vector<CalendarEventSaveRequest>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().repeatSeriesId),
        std::optional<CalendarEventSeriesId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().eventType),
        CalendarEventType
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().startDate),
        GregorianDate
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().editedStartDate),
        GregorianDate
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().editedEndDate),
        GregorianDate
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
        CalendarEventTypeName
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventSummary>().timeStatus),
        CalendarEventTimeStatus
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventSummary>().repeatSeriesId),
        std::optional<CalendarEventSeriesId>
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

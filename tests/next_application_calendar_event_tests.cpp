#include "next/application/calendar_event_campus_visibility_policy.h"
#include "next/application/calendar_event_delete_port.h"
#include "next/application/calendar_event_delete_use_case.h"
#include "next/application/calendar_event_delete_all_port.h"
#include "next/application/calendar_event_delete_all_use_case.h"
#include "next/application/calendar_event_edit_draft.h"
#include "next/application/calendar_event_import_save_port.h"
#include "next/application/calendar_event_projection.h"
#include "next/application/calendar_event_repeat_occurrence_plan.h"
#include "next/application/calendar_event_save_port.h"
#include "next/application/calendar_event_save_use_case.h"
#include "next/application/calendar_event_start_of_term_policy.h"
#include "next/application/calendar_event_series_create_port.h"
#include "next/application/calendar_event_series_create_use_case.h"
#include "next/application/calendar_event_series_delete_port.h"
#include "next/application/calendar_event_series_delete_use_case.h"
#include "next/application/calendar_event_series_edit_port.h"
#include "next/application/calendar_event_series_edit_plan.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

CalendarEventId calendarEventId(
    std::string value
    )
{
    return *CalendarEventId::fromString(value);
}

ClassId classId(
    std::string value
    )
{
    return *ClassId::fromString(value);
}

CampusId campusId(
    std::string value
    )
{
    return *CampusId::fromString(value);
}

CalendarEventSummary calendarEventSummary(
    std::string id,
    const std::int32_t order = 0,
    const bool allDay = false
    )
{
    CalendarEventSummary event{
        calendarEventId(std::move(id)),
        std::nullopt,
        std::nullopt,
        "Event title",
        "2026-09-20",
        "2026-09-20",
        std::string("09:00"),
        std::string("10:00"),
        "Room 1",
        "Event notes",
        order,
        allDay
    };

    if (allDay)
    {
        event.startTime.reset();
        event.endTime.reset();
    }

    return event;
}

CalendarEventSaveRequest validSaveRequest()
{
    return CalendarEventSaveRequest{
        std::nullopt,
        "Event title",
        "2026-09-20",
        "2026-09-20",
        std::string("09:00"),
        std::string("10:00"),
        false,
        "Meeting",
        "Timed"
    };
}

CalendarEventEditDraft validEditDraft()
{
    return CalendarEventEditDraft{
        calendarEventId("event-42"),
        std::string("series-1"),
        "Event title",
        "2026-09-20",
        "2026-09-20",
        std::string("09:00"),
        std::string("10:00"),
        false,
        "Meeting",
        "Timed"
    };
}

CalendarEventSeriesCreateRequest validSeriesCreateRequest()
{
    return CalendarEventSeriesCreateRequest{
        "series-create",
        {validSaveRequest()}
    };
}

CalendarEventSeriesEditRequest validSeriesEditRequest()
{
    return CalendarEventSeriesEditRequest{
        "series-1",
        "2026-09-20",
        "2026-09-22",
        "2026-09-23",
        "Edited event title",
        std::string("09:00"),
        std::string("10:00"),
        false,
        "Meeting",
        "Timed"
    };
}

CalendarEventProjectionInput validInput()
{
    CalendarEventProjectionInput input;
    input.events = {
        calendarEventSummary("event-1", 8),
        calendarEventSummary("event-2", 16, true)
    };
    input.events[0].classId = classId("class-1");
    input.events[0].campusId = campusId("campus-1");
    input.events[0].title = "Staff meeting";
    input.events[0].startDate = "2026-09-20";
    input.events[0].endDate = "2026-09-20";
    input.events[0].eventType = "Meeting";
    input.events[0].timeStatus = "Timed";
    input.events[0].repeatSeriesId = "series-1";
    input.events[0].location = "Main campus";
    input.events[0].notes = "Bring the agenda";
    input.events[1].title = "Campus holiday";
    input.events[1].startDate = "2026-09-21";
    input.events[1].endDate = "2026-09-22";
    input.events[1].eventType = "Holiday";
    input.events[1].timeStatus = "Timed";
    input.events[1].repeatSeriesId.reset();
    input.events[1].location.clear();
    input.events[1].notes.clear();
    return input;
}

void verifyInvalid(
    const Result<CalendarEventProjection>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

void verifyInvalidValidation(
    const Result<void>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

void verifyInvalidValidation(
    const Result<void>& result,
    const std::string_view expectedMessage
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(result.error().message, std::string(expectedMessage));
    QVERIFY(!result.error().recoverable);
}

void verifyInvalidRepeatPlan(
    const Result<CalendarEventSeriesCreateRequest>& result
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

void verifyOccurrenceStartDates(
    const CalendarEventSeriesCreateRequest& request,
    const std::initializer_list<std::string_view> expected
    )
{
    QCOMPARE(request.occurrences.size(), expected.size());
    std::size_t index = 0;
    for (const std::string_view date : expected)
    {
        QCOMPARE(request.occurrences.at(index).startDate, std::string(date));
        ++index;
    }
}

template <typename Value>
concept HasRawSourceAccessor = requires(const Value& value)
{
    value.rawSource();
};

std::vector<CalendarEventCampusCode> campusCodes(
    std::initializer_list<std::string_view> values
    )
{
    std::vector<CalendarEventCampusCode> result;
    result.reserve(values.size());

    for (const std::string_view value : values)
    {
        std::string normalized(value);
        std::string caseFolded = normalized;
        for (char& character : caseFolded)
        {
            if (character >= 'A' && character <= 'Z')
            {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }
        result.push_back({std::move(normalized), std::move(caseFolded)});
    }

    return result;
}

class FakeCalendarEventSeriesCreatePort final
    : public CalendarEventSeriesCreatePort
{
public:
    int createCalls = 0;
    std::optional<CalendarEventSeriesCreateRequest> receivedRequest;
    CalendarEventSeriesCreateResult response =
        CalendarEventSeriesCreateResult::success({});

    CalendarEventSeriesCreateResult createRepeatSeries(
        const CalendarEventSeriesCreateRequest& request
        ) override
    {
        ++createCalls;
        receivedRequest = request;
        return response;
    }
};

class FakeCalendarEventSeriesDeletePort final
    : public CalendarEventSeriesDeletePort
{
public:
    int deleteCalls = 0;
    std::optional<CalendarEventSeriesDeleteRequest> receivedRequest;
    CalendarEventSeriesDeleteResult response =
        CalendarEventSeriesDeleteResult::success();

    CalendarEventSeriesDeleteResult deleteRepeatSeriesFromDate(
        const CalendarEventSeriesDeleteRequest& request
        ) override
    {
        ++deleteCalls;
        receivedRequest = request;
        return response;
    }
};

class FakeCalendarEventDeletePort final : public CalendarEventDeletePort
{
public:
    int deleteCalls = 0;
    std::optional<CalendarEventId> receivedEventId;
    CalendarEventDeleteResult response = CalendarEventDeleteResult::success();

    CalendarEventDeleteResult deleteEvent(
        const CalendarEventId& eventId
        ) override
    {
        ++deleteCalls;
        receivedEventId = eventId;
        return response;
    }
};

class FakeCalendarEventDeleteAllPort final : public CalendarEventDeleteAllPort
{
public:
    mutable int availabilityCalls = 0;
    bool available = true;
    int deleteCalls = 0;
    CalendarEventDeleteAllResult response =
        CalendarEventDeleteAllResult::success();

    bool isAvailable() const override
    {
        ++availabilityCalls;
        return available;
    }

    CalendarEventDeleteAllResult deleteAllEvents() override
    {
        ++deleteCalls;
        return response;
    }
};

class FakeCalendarEventSavePort final : public CalendarEventSavePort
{
public:
    int saveCalls = 0;
    std::optional<CalendarEventSaveRequest> receivedRequest;
    CalendarEventSaveResult response =
        CalendarEventSaveResult::success(calendarEventId("saved-event"));

    CalendarEventSaveResult saveEvent(
        const CalendarEventSaveRequest& request
        ) override
    {
        ++saveCalls;
        receivedRequest = request;
        return response;
    }
};

}

class NextApplicationCalendarEventTests final : public QObject
{
    Q_OBJECT

private slots:
    void valid96ScaleEventsRetainTypedMetadataAndBounds();
    void typedReferencesAndTemporalFieldsRemainExplicit();
    void allDayAndOptionalTimePolicyIsDeterministic();
    void eventClassificationAndRepeatSeriesFieldsRemainBounded();
    void optionalReferencesAndMetadataAreRetained();
    void emptyProjectionAndMissingLookupAreExplicit();
    void orderingAndSafeValueLookupsAreDeterministic();
    void duplicateAndInvalidValuesReturnStructuredInputErrors();
    void exactCapsAreAcceptedAndOverflowIsRejected();
    void saveRequestBoundsAndOptionalIdRemainTyped();
    void saveRequestAllDayAndTimeStatusPolicyIsExplicit();
    void calendarEventSaveUseCaseForwardsRequestAndResultOnce();
    void calendarEventSaveUseCasePropagatesPortFailure();
    void calendarEventSaveUseCaseRejectsInvalidRequestWithoutCallingPort();
    void timingErrorsKeepFeatureMessagesAndValidationPrecedence();
    void calendarEventNameValidationUsesDomainClassifiersAndPreservesRawText();
    void timingAcceptsFullGregorianRangeAndCrossDayClocks();
    void saveRequestContractHasNoQtOrLegacySurface();
    void importSaveRequestBoundsOrderedCreateBatchAndNoOp();
    void importSaveRequestRejectsUpdatesAndInvalidEvents();
    void importSaveRequestContractHasNoQtOrLegacySurface();
    void deleteAllPortContractUsesStructuredQtFreeResult();
    void deleteAllUseCaseForwardsAvailability();
    void deleteAllUseCaseDeletesOnceAndReturnsSuccess();
    void deleteAllUseCasePropagatesPortFailure();
    void editDraftBoundsAndTimeStatusPolicyIsExplicit();
    void editDraftIsCopyableEqualAndIndependentlyReleasable();
    void editDraftContractHasNoQtOrLegacySurface();
    void seriesCreateRequestBoundsAndOccurrencesRemainTyped();
    void seriesCreateRequestContractHasNoQtOrLegacySurface();
    void repeatOccurrencePlanSupportsDailyWeeklyAndInclusiveUntil();
    void repeatOccurrencePlanClampsMonthlyDatesFromPreviousOccurrence();
    void repeatOccurrencePlanPreservesSeedFieldsAndDateDuration();
    void repeatOccurrencePlanRejectsInvalidSeedRangeUntilAndFrequency();
    void repeatOccurrencePlanAccepts366AndRejects367Occurrences();
    void repeatOccurrencePlanHasTypedQtFreeContract();
    void repeatSeriesCreateUseCasePlansAndForwardsExactlyOnce();
    void repeatSeriesCreateUseCasePropagatesPortFailure();
    void repeatSeriesCreateUseCaseRejectsInvalidPlanWithoutCallingPort();
    void seriesDeleteRequestValidationIsCanonicalAndQtFree();
    void repeatSeriesDeleteUseCaseForwardsRequestAndResultOnce();
    void repeatSeriesDeleteUseCasePropagatesPortFailure();
    void repeatSeriesDeleteUseCaseRejectsInvalidRequestWithoutCallingPort();
    void singleEventDeleteUseCaseForwardsIdAndResultOnce();
    void singleEventDeleteUseCasePropagatesPortFailure();
    void seriesEditRequestBoundsAndDatesRemainTyped();
    void seriesEditRequestAllDayAndTimeStatusPolicyIsExplicit();
    void seriesEditRequestContractHasNoQtOrLegacySurface();
    void seriesEditPlanPreservesOrderAndPropagatesRequestFields();
    void seriesEditPlanHandlesEmptyAndAllDaySuffixes();
    void seriesEditPlanRejectsInvalidSourceAndOutOfRangeDates();
    void recordsAndProjectionAreCopyableEqualAndIndependentlyReleasable();
    void contractHasNoMutablePointerOrRichRecordSurface();
    void campusVisibilityPolicyMatchesNormalizedLiteralCampusTokens();
    void startOfTermPolicyMatchesCalendarClassificationAndHideSwitch();
};

void NextApplicationCalendarEventTests::valid96ScaleEventsRetainTypedMetadataAndBounds()
{
    CalendarEventProjectionInput input;
    input.events.reserve(96);
    for (std::size_t index = 0; index < 96; ++index)
    {
        const bool allDay = index % 3 == 0;
        auto event = calendarEventSummary(
            "event-" + std::to_string(index + 1),
            static_cast<std::int32_t>(index * 2),
            allDay
            );
        event.title = "Event " + std::to_string(index + 1);
        event.startDate = "2026-09-" + std::to_string(10 + index % 20);
        event.endDate = event.startDate;
        event.location = index % 2 == 0 ? "Room A" : "Room B";
        event.notes = index % 4 == 0 ? "Metadata" : "";
        if (index % 2 == 0)
        {
            event.classId = classId("class-" + std::to_string(index + 1));
        }
        if (index % 5 == 0)
        {
            event.campusId = campusId("campus-" + std::to_string(index + 1));
        }
        input.events.push_back(std::move(event));
    }

    const auto result = CalendarEventProjection::create(std::move(input));

    QVERIFY(result);
    const auto& projection = result.value();
    QCOMPARE(projection.eventCount(), std::size_t(96));
    QCOMPARE(projection.count(), std::size_t(96));
    QCOMPARE(projection.size(), std::size_t(96));
    QCOMPARE(projection.events().size(), std::size_t(96));
    QCOMPARE(projection.summaries().size(), std::size_t(96));
    QCOMPARE(projection.entries().size(), std::size_t(96));
    QCOMPARE(projection.calendarEvents().size(), std::size_t(96));
    QVERIFY(!projection.empty());
    QCOMPARE(
        projection.events().front().id.value(),
        std::string("event-1")
        );
    QCOMPARE(projection.events().front().order, std::int32_t(0));
    QVERIFY(projection.events().front().allDay);
    QVERIFY(!projection.events().front().hasTimeRange());
    QVERIFY(projection.events().at(1).hasTimeRange());
    QCOMPARE(projection.events().back().order, std::int32_t(190));
}

void NextApplicationCalendarEventTests::typedReferencesAndTemporalFieldsRemainExplicit()
{
    static_assert(!std::is_same_v<CalendarEventId, ClassId>);
    static_assert(!std::is_same_v<CalendarEventId, CampusId>);
    static_assert(!std::is_same_v<ClassId, CampusId>);
    static_assert(!std::is_convertible_v<CalendarEventId, ClassId>);
    static_assert(!std::is_convertible_v<ClassId, CalendarEventId>);
    static_assert(!std::is_convertible_v<CampusId, CalendarEventId>);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().id),
        CalendarEventId
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().classId),
        std::optional<ClassId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().campusId),
        std::optional<CampusId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().startDate),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().startTime),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().allDay),
        bool
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().eventType),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().timeStatus),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSummary>().repeatSeriesId),
        std::optional<std::string>
        >);

    const auto result = CalendarEventProjection::create(validInput());

    QVERIFY(result);
    const auto event = result.value().findEvent(calendarEventId("event-1"));
    QVERIFY(event.has_value());
    QVERIFY(event->classId.has_value());
    QVERIFY(event->campusId.has_value());
    QCOMPARE(event->classId->value(), std::string("class-1"));
    QCOMPARE(event->campusId->value(), std::string("campus-1"));
    QCOMPARE(event->startDate, std::string("2026-09-20"));
    QCOMPARE(event->endDate, std::string("2026-09-20"));
    QCOMPARE(event->startTime.value(), std::string("09:00"));
    QCOMPARE(event->endTime.value(), std::string("10:00"));
    QCOMPARE(event->eventType, std::string("Meeting"));
    QCOMPARE(event->timeStatus, std::string("Timed"));
    QVERIFY(event->hasRepeatSeries());
    QVERIFY(event->hasRepeatSeriesId());
    QCOMPARE(event->repeatSeriesId.value(), std::string("series-1"));
    QVERIFY(!event->allDay);
}

void NextApplicationCalendarEventTests::allDayAndOptionalTimePolicyIsDeterministic()
{
    auto allDay = calendarEventSummary("all-day", 1, true);
    QVERIFY(!allDay.startTime.has_value());
    QVERIFY(!allDay.endTime.has_value());
    QVERIFY(CalendarEventProjection::validate(allDay));
    QVERIFY(CalendarEventProjection::create({{allDay}}));

    auto allDayWithStartTime = allDay;
    allDayWithStartTime.startTime = "09:00";
    verifyInvalid(
        CalendarEventProjection::create({{std::move(allDayWithStartTime)}})
        );

    auto allDayWithEndTime = allDay;
    allDayWithEndTime.endTime = "10:00";
    verifyInvalid(
        CalendarEventProjection::create({{std::move(allDayWithEndTime)}})
        );

    auto partialTimedRange = calendarEventSummary("partial-time");
    partialTimedRange.endTime.reset();
    verifyInvalid(
        CalendarEventProjection::create({{std::move(partialTimedRange)}})
        );

    auto unknownTimedEvent = calendarEventSummary("unknown-time");
    unknownTimedEvent.timeStatus = "Unknown";
    unknownTimedEvent.startTime.reset();
    unknownTimedEvent.endTime.reset();
    QVERIFY(CalendarEventProjection::validate(unknownTimedEvent));
    QVERIFY(CalendarEventProjection::create({{std::move(unknownTimedEvent)}}));
}

void NextApplicationCalendarEventTests::
eventClassificationAndRepeatSeriesFieldsRemainBounded()
{
    const auto result = CalendarEventProjection::create(validInput());
    QVERIFY(result);
    const auto event = result.value().events().front();
    QCOMPARE(event.eventType, std::string("Meeting"));
    QCOMPARE(event.timeStatus, std::string("Timed"));
    QCOMPARE(event.repeatSeriesId.value(), std::string("series-1"));

    auto exactFields = validInput();
    auto& exactEvent = exactFields.events.front();
    exactEvent.eventType = std::string(
        kCalendarEventSummaryMaxEventTypeLength,
        'e'
        );
    exactEvent.timeStatus = std::string(
        kCalendarEventSummaryMaxTimeStatusLength,
        's'
        );
    exactEvent.repeatSeriesId = std::string(
        kCalendarEventSummaryMaxRepeatSeriesIdLength,
        'r'
        );
    const auto exactResult = CalendarEventProjection::create(
        std::move(exactFields)
        );
    QVERIFY(exactResult);
    QCOMPARE(
        exactResult.value().events().front().eventType.size(),
        kCalendarEventSummaryMaxEventTypeLength
        );
    QCOMPARE(
        exactResult.value().events().front().timeStatus.size(),
        kCalendarEventSummaryMaxTimeStatusLength
        );
    QCOMPARE(
        exactResult.value().events().front().repeatSeriesId->size(),
        kCalendarEventSummaryMaxRepeatSeriesIdLength
        );

    {
        auto input = validInput();
        input.events.front().eventType = std::string(
            kCalendarEventSummaryMaxEventTypeLength + 1,
            'e'
            );
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().timeStatus = std::string(
            kCalendarEventSummaryMaxTimeStatusLength + 1,
            's'
            );
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().repeatSeriesId = std::string(
            kCalendarEventSummaryMaxRepeatSeriesIdLength + 1,
            'r'
            );
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().eventType = " \t";
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().timeStatus = "\n";
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().repeatSeriesId = std::string(" ");
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    auto noRepeat = validInput().events.back();
    noRepeat.repeatSeriesId.reset();
    QVERIFY(CalendarEventProjection::validate(noRepeat));
    QVERIFY(!noRepeat.hasRepeatSeries());
}

void NextApplicationCalendarEventTests::optionalReferencesAndMetadataAreRetained()
{
    const auto result = CalendarEventProjection::create(validInput());
    QVERIFY(result);

    const auto first = result.value().lookupById(calendarEventId("event-1"));
    const auto second = result.value().lookupEvent(calendarEventId("event-2"));
    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    QVERIFY(first->hasClass());
    QVERIFY(first->hasCampus());
    QVERIFY(!second->classId.has_value());
    QVERIFY(!second->campusId.has_value());
    QCOMPARE(first->title, std::string("Staff meeting"));
    QCOMPARE(first->location, std::string("Main campus"));
    QCOMPARE(first->notes, std::string("Bring the agenda"));
    QVERIFY(second->location.empty());
    QVERIFY(second->notes.empty());
    QVERIFY(second->isAllDay());
    QVERIFY(!second->hasTimeRange());
}

void NextApplicationCalendarEventTests::emptyProjectionAndMissingLookupAreExplicit()
{
    const CalendarEventProjectionInput emptyInput;
    QVERIFY(CalendarEventProjection::validate(emptyInput));

    const auto result = CalendarEventProjection::create(emptyInput);
    QVERIFY(result);
    const auto& projection = result.value();
    QVERIFY(projection.empty());
    QCOMPARE(projection.eventCount(), std::size_t(0));
    QVERIFY(projection.events().empty());
    QVERIFY(!projection.findEvent(calendarEventId("missing")).has_value());
    QVERIFY(!projection.lookupEvent(calendarEventId("missing")).has_value());
    QVERIFY(!projection.findById(calendarEventId("missing")).has_value());
    QVERIFY(!projection.lookupById(calendarEventId("missing")).has_value());
    QVERIFY(!projection.find(calendarEventId("missing")).has_value());
    QVERIFY(!projection.lookup(calendarEventId("missing")).has_value());

    const CalendarEventProjection defaultProjection;
    QVERIFY(defaultProjection.empty());
    QVERIFY(defaultProjection == projection);
}

void NextApplicationCalendarEventTests::orderingAndSafeValueLookupsAreDeterministic()
{
    auto input = validInput();
    input.events.front().order = 42;
    input.events.back().order = 7;
    const auto result = CalendarEventProjection::create(std::move(input));
    QVERIFY(result);

    const auto& projection = result.value();
    QCOMPARE(projection.events().front().order, std::int32_t(42));
    QCOMPARE(projection.events().back().order, std::int32_t(7));

    auto eventCopy = projection.findEvent(calendarEventId("event-1"));
    QVERIFY(eventCopy.has_value());
    eventCopy->title = "Changed outside projection";
    eventCopy->classId.reset();
    eventCopy->order = 99;
    eventCopy->eventType = "Changed type";
    eventCopy->repeatSeriesId.reset();

    const auto stored = projection.findEvent(calendarEventId("event-1"));
    QVERIFY(stored.has_value());
    QCOMPARE(stored->title, std::string("Staff meeting"));
    QVERIFY(stored->classId.has_value());
    QCOMPARE(stored->order, std::int32_t(42));
    QCOMPARE(stored->eventType, std::string("Meeting"));
    QCOMPARE(stored->repeatSeriesId.value(), std::string("series-1"));
    QCOMPARE(
        projection.findById(calendarEventId("event-1"))->id.value(),
        std::string("event-1")
        );
}

void NextApplicationCalendarEventTests::duplicateAndInvalidValuesReturnStructuredInputErrors()
{
    {
        auto input = validInput();
        input.events.push_back(calendarEventSummary("event-1"));
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().id = calendarEventId(" \t");
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().classId = classId(" ");
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().campusId = campusId(
            std::string(kCalendarEventSummaryMaxIdentifierLength + 1, 'c')
            );
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().title = "\n";
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().startDate.clear();
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().endDate = std::string(
            kCalendarEventSummaryMaxDateLength + 1,
            'd'
            );
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().location = " \t";
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().notes = std::string(
            kCalendarEventSummaryMaxNotesLength + 1,
            'n'
            );
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().startTime = " ";
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().endTime = std::string(
            kCalendarEventSummaryMaxTimeLength + 1,
            't'
            );
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().eventType.clear();
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().timeStatus = " ";
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().repeatSeriesId = std::string();
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    {
        auto input = validInput();
        input.events.front().order = -1;
        verifyInvalid(CalendarEventProjection::create(std::move(input)));
    }

    auto invalidEvent = validInput().events.front();
    invalidEvent.title.clear();
    verifyInvalidValidation(CalendarEventProjection::validate(invalidEvent));
}

void NextApplicationCalendarEventTests::exactCapsAreAcceptedAndOverflowIsRejected()
{
    CalendarEventProjectionInput exactInput;
    exactInput.events.reserve(kCalendarEventProjectionMaxEvents);
    for (std::size_t index = 0;
         index < kCalendarEventProjectionMaxEvents;
         ++index)
    {
        exactInput.events.push_back(
            calendarEventSummary(
                "event-cap-" + std::to_string(index),
                static_cast<std::int32_t>(index)
                )
            );
    }

    QVERIFY(CalendarEventProjection::validate(exactInput));
    const auto exactResult = CalendarEventProjection::create(
        std::move(exactInput)
        );
    QVERIFY(exactResult);
    QCOMPARE(
        exactResult.value().eventCount(),
        kCalendarEventProjectionMaxEvents
        );

    auto overflowInput = validInput();
    overflowInput.events.reserve(kCalendarEventProjectionMaxEvents + 1);
    for (std::size_t index = overflowInput.events.size();
         index < kCalendarEventProjectionMaxEvents + 1;
         ++index)
    {
        overflowInput.events.push_back(
            calendarEventSummary(
                "overflow-event-" + std::to_string(index),
                static_cast<std::int32_t>(index)
                )
            );
    }
    verifyInvalid(
        CalendarEventProjection::create(std::move(overflowInput))
        );

    auto exactFields = validInput();
    auto& event = exactFields.events.front();
    event.id = calendarEventId(
        std::string(kCalendarEventSummaryMaxIdentifierLength, 'i')
        );
    event.classId = classId(
        std::string(kCalendarEventSummaryMaxIdentifierLength, 'c')
        );
    event.campusId = campusId(
        std::string(kCalendarEventSummaryMaxIdentifierLength, 'p')
        );
    event.title = std::string(kCalendarEventSummaryMaxTitleLength, 't');
    event.startDate = std::string(kCalendarEventSummaryMaxDateLength, 'd');
    event.endDate = std::string(kCalendarEventSummaryMaxDateLength, 'e');
    event.startTime = std::string(kCalendarEventSummaryMaxTimeLength, 's');
    event.endTime = std::string(kCalendarEventSummaryMaxTimeLength, 'e');
    event.eventType = std::string(
        kCalendarEventSummaryMaxEventTypeLength,
        'y'
        );
    event.timeStatus = std::string(
        kCalendarEventSummaryMaxTimeStatusLength,
        'z'
        );
    event.repeatSeriesId = std::string(
        kCalendarEventSummaryMaxRepeatSeriesIdLength,
        'r'
        );
    event.location = std::string(kCalendarEventSummaryMaxLocationLength, 'l');
    event.notes = std::string(kCalendarEventSummaryMaxNotesLength, 'n');

    const auto exactFieldResult = CalendarEventProjection::create(
        std::move(exactFields)
        );
    QVERIFY(exactFieldResult);
    const auto exactEvent = exactFieldResult.value().events().front();
    QCOMPARE(
        exactEvent.id.value().size(),
        kCalendarEventSummaryMaxIdentifierLength
        );
    QCOMPARE(exactEvent.title.size(), kCalendarEventSummaryMaxTitleLength);
    QCOMPARE(exactEvent.startDate.size(), kCalendarEventSummaryMaxDateLength);
    QCOMPARE(exactEvent.startTime->size(), kCalendarEventSummaryMaxTimeLength);
    QCOMPARE(
        exactEvent.eventType.size(),
        kCalendarEventSummaryMaxEventTypeLength
        );
    QCOMPARE(
        exactEvent.timeStatus.size(),
        kCalendarEventSummaryMaxTimeStatusLength
        );
    QCOMPARE(
        exactEvent.repeatSeriesId->size(),
        kCalendarEventSummaryMaxRepeatSeriesIdLength
        );
    QCOMPARE(exactEvent.location.size(), kCalendarEventSummaryMaxLocationLength);
    QCOMPARE(exactEvent.notes.size(), kCalendarEventSummaryMaxNotesLength);
}

void NextApplicationCalendarEventTests::
saveRequestBoundsAndOptionalIdRemainTyped()
{
    auto createRequest = validSaveRequest();
    QVERIFY(!createRequest.id.has_value());
    QVERIFY(createRequest.validate());
    QVERIFY(validateCalendarEventSaveRequest(createRequest));

    auto updateRequest = createRequest;
    updateRequest.id = calendarEventId("event-42");
    QVERIFY(updateRequest.validate());

    auto invalidId = createRequest;
    invalidId.id = calendarEventId("   ");
    QVERIFY(!invalidId.validate());
    QCOMPARE(invalidId.validate().error().code, ErrorCode::InvalidInput);

    auto oversizedId = createRequest;
    oversizedId.id = calendarEventId(
        std::string(kCalendarEventSaveMaxIdentifierLength + 1, 'i')
        );
    QVERIFY(!oversizedId.validate());

    auto exactTitle = createRequest;
    exactTitle.title = std::string(kCalendarEventSaveMaxTitleLength, 't');
    QVERIFY(exactTitle.validate());

    auto oversizedTitle = exactTitle;
    oversizedTitle.title.push_back('x');
    QVERIFY(!oversizedTitle.validate());

    auto oversizedEventType = createRequest;
    oversizedEventType.eventType = std::string(
        kCalendarEventSaveMaxEventTypeLength + 1,
        'e'
        );
    QVERIFY(!oversizedEventType.validate());

    auto oversizedTimeStatus = createRequest;
    oversizedTimeStatus.timeStatus = std::string(
        kCalendarEventSaveMaxTimeStatusLength + 1,
        's'
        );
    QVERIFY(!oversizedTimeStatus.validate());

    auto invalidDate = createRequest;
    invalidDate.startDate = "2026-02-30";
    QVERIFY(!invalidDate.validate());

    auto reversedDates = createRequest;
    reversedDates.startDate = "2026-09-21";
    QVERIFY(!reversedDates.validate());
}

void NextApplicationCalendarEventTests::
saveRequestAllDayAndTimeStatusPolicyIsExplicit()
{
    auto allDay = validSaveRequest();
    allDay.allDay = true;
    allDay.timeStatus = "Timed";
    allDay.startTime.reset();
    allDay.endTime.reset();
    QVERIFY(allDay.validate());

    auto allDayWithTime = allDay;
    allDayWithTime.startTime = "09:00";
    allDayWithTime.endTime = "10:00";
    QVERIFY(!allDayWithTime.validate());

    auto allDayUnknown = allDay;
    allDayUnknown.timeStatus = "Unknown";
    QVERIFY(!allDayUnknown.validate());

    auto timedWithoutTimes = validSaveRequest();
    timedWithoutTimes.startTime.reset();
    timedWithoutTimes.endTime.reset();
    QVERIFY(!timedWithoutTimes.validate());

    auto partialTimedRange = validSaveRequest();
    partialTimedRange.endTime.reset();
    QVERIFY(!partialTimedRange.validate());

    auto reversedTimedRange = validSaveRequest();
    reversedTimedRange.endTime = "08:00";
    QVERIFY(!reversedTimedRange.validate());

    auto unknownTime = validSaveRequest();
    unknownTime.timeStatus = "Unknown";
    unknownTime.startTime.reset();
    unknownTime.endTime.reset();
    QVERIFY(unknownTime.validate());

    auto unconfirmedTime = unknownTime;
    unconfirmedTime.timeStatus = "Unconfirmed";
    QVERIFY(unconfirmedTime.validate());

    auto unknownWithTime = unknownTime;
    unknownWithTime.startTime = "09:00";
    unknownWithTime.endTime = "10:00";
    QVERIFY(!unknownWithTime.validate());
}

void NextApplicationCalendarEventTests::
calendarEventSaveUseCaseForwardsRequestAndResultOnce()
{
    using UseCaseResult = decltype(CalendarEventSaveUseCase::execute(
        std::declval<CalendarEventSavePort&>(),
        std::declval<const CalendarEventSaveRequest&>()
        ));
    static_assert(std::is_same_v<UseCaseResult, CalendarEventSaveResult>);

    CalendarEventSaveRequest request = validSaveRequest();
    request.id = calendarEventId("event-42");
    request.title = "Detached occurrence update";

    FakeCalendarEventSavePort port;
    const CalendarEventId savedId = calendarEventId("event-42");
    port.response = CalendarEventSaveResult::success(savedId);

    const CalendarEventSaveResult result =
        CalendarEventSaveUseCase::execute(port, request);

    QVERIFY(result);
    QCOMPARE(port.saveCalls, 1);
    QVERIFY(port.receivedRequest.has_value());
    QVERIFY(*port.receivedRequest == request);
    QVERIFY(result.value() == savedId);
}

void NextApplicationCalendarEventTests::
calendarEventSaveUseCasePropagatesPortFailure()
{
    FakeCalendarEventSavePort port;
    port.response = CalendarEventSaveResult::failure({
        .code = ErrorCode::Conflict,
        .message = "The calendar event changed before it could be saved.",
        .recoverable = true
    });

    const CalendarEventSaveResult result =
        CalendarEventSaveUseCase::execute(port, validSaveRequest());

    QVERIFY(!result);
    QCOMPARE(port.saveCalls, 1);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QCOMPARE(
        result.error().message,
        std::string("The calendar event changed before it could be saved.")
        );
    QVERIFY(result.error().recoverable);
}

void NextApplicationCalendarEventTests::
calendarEventSaveUseCaseRejectsInvalidRequestWithoutCallingPort()
{
    FakeCalendarEventSavePort port;
    CalendarEventSaveRequest request = validSaveRequest();
    request.title = "   ";

    const CalendarEventSaveResult result =
        CalendarEventSaveUseCase::execute(port, request);

    QVERIFY(!result);
    QCOMPARE(port.saveCalls, 0);
    QVERIFY(!port.receivedRequest.has_value());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        result.error().message,
        std::string("Calendar event text fields must be non-blank and bounded.")
        );
    QVERIFY(!result.error().recoverable);
}

void NextApplicationCalendarEventTests::
timingErrorsKeepFeatureMessagesAndValidationPrecedence()
{
    using TimingCase = std::pair<CalendarEventSaveRequest, std::string_view>;
    std::vector<TimingCase> saveCases;
    const auto addSaveCase = [&saveCases](
        const auto& mutate,
        const std::string_view message
        )
    {
        auto request = validSaveRequest();
        mutate(request);
        saveCases.emplace_back(std::move(request), message);
    };
    addSaveCase(
        [](auto& request) { request.startDate = "2026-02-30"; },
        "Calendar event dates must be valid canonical ISO dates."
        );
    addSaveCase(
        [](auto& request) { request.startDate = "2026-09-21"; },
        "Calendar event end date must not precede its start date."
        );
    addSaveCase(
        [](auto& request) { request.endTime.reset(); },
        "Calendar event start and end times must be both absent or both present."
        );
    addSaveCase(
        [](auto& request) { request.startTime = "9:00"; },
        "Calendar event times must be valid canonical 24-hour times."
        );
    addSaveCase(
        [](auto& request)
        {
            request.allDay = true;
            request.timeStatus = "Unknown";
            request.startTime.reset();
            request.endTime.reset();
        },
        "All-day calendar events require Timed status and no times."
        );
    addSaveCase(
        [](auto& request)
        {
            request.startTime.reset();
            request.endTime.reset();
        },
        "Timed calendar events require both start and end times."
        );
    addSaveCase(
        [](auto& request) { request.endTime = "09:00"; },
        "Calendar event end time must be after its start time."
        );
    addSaveCase(
        [](auto& request) { request.timeStatus = "Unknown"; },
        "Unknown or unconfirmed calendar events require no times."
        );
    for (const auto& [request, message] : saveCases)
    {
        verifyInvalidValidation(request.validate(), message);
    }

    auto saveTextPrecedence = validSaveRequest();
    saveTextPrecedence.title.clear();
    saveTextPrecedence.startDate = "2026-02-30";
    verifyInvalidValidation(
        saveTextPrecedence.validate(),
        "Calendar event text fields must be non-blank and bounded."
        );

    auto saveDatePrecedence = validSaveRequest();
    saveDatePrecedence.startDate = "2026-02-30";
    saveDatePrecedence.endTime.reset();
    verifyInvalidValidation(
        saveDatePrecedence.validate(),
        "Calendar event dates must be valid canonical ISO dates."
        );

    auto saveRangePrecedence = validSaveRequest();
    saveRangePrecedence.startDate = "2026-09-21";
    saveRangePrecedence.endTime.reset();
    verifyInvalidValidation(
        saveRangePrecedence.validate(),
        "Calendar event end date must not precede its start date."
        );

    auto savePairPrecedence = validSaveRequest();
    savePairPrecedence.startTime = "9:00";
    savePairPrecedence.endTime.reset();
    verifyInvalidValidation(
        savePairPrecedence.validate(),
        "Calendar event start and end times must be both absent or both present."
        );

    using DraftCase = std::pair<CalendarEventEditDraft, std::string_view>;
    std::vector<DraftCase> draftCases;
    const auto addDraftCase = [&draftCases](
        const auto& mutate,
        const std::string_view message
        )
    {
        auto draft = validEditDraft();
        mutate(draft);
        draftCases.emplace_back(std::move(draft), message);
    };
    addDraftCase(
        [](auto& draft) { draft.startDate = "2026-02-30"; },
        "Calendar event draft dates must be valid canonical ISO dates."
        );
    addDraftCase(
        [](auto& draft) { draft.startDate = "2026-09-21"; },
        "Calendar event draft end date must not precede its start date."
        );
    addDraftCase(
        [](auto& draft) { draft.endTime.reset(); },
        "Calendar event draft start and end times must be both absent or both present."
        );
    addDraftCase(
        [](auto& draft) { draft.startTime = "9:00"; },
        "Calendar event draft times must be valid canonical 24-hour times."
        );
    addDraftCase(
        [](auto& draft)
        {
            draft.allDay = true;
            draft.timeStatus = "Unknown";
            draft.startTime.reset();
            draft.endTime.reset();
        },
        "All-day calendar event drafts require Timed status and no times."
        );
    addDraftCase(
        [](auto& draft)
        {
            draft.startTime.reset();
            draft.endTime.reset();
        },
        "Timed calendar event drafts require both start and end times."
        );
    addDraftCase(
        [](auto& draft) { draft.endTime = "09:00"; },
        "Calendar event draft end time must be after its start time."
        );
    addDraftCase(
        [](auto& draft) { draft.timeStatus = "Unknown"; },
        "Unknown or unconfirmed calendar event drafts require no times."
        );
    for (const auto& [draft, message] : draftCases)
    {
        verifyInvalidValidation(draft.validate(), message);
    }

    auto draftTextPrecedence = validEditDraft();
    draftTextPrecedence.title.clear();
    draftTextPrecedence.startDate = "2026-02-30";
    verifyInvalidValidation(
        draftTextPrecedence.validate(),
        "Calendar event draft text fields must be non-blank and bounded."
        );

    auto draftDatePrecedence = validEditDraft();
    draftDatePrecedence.startDate = "2026-02-30";
    draftDatePrecedence.endTime.reset();
    verifyInvalidValidation(
        draftDatePrecedence.validate(),
        "Calendar event draft dates must be valid canonical ISO dates."
        );

    auto draftRangePrecedence = validEditDraft();
    draftRangePrecedence.startDate = "2026-09-21";
    draftRangePrecedence.endTime.reset();
    verifyInvalidValidation(
        draftRangePrecedence.validate(),
        "Calendar event draft end date must not precede its start date."
        );

    auto draftPairPrecedence = validEditDraft();
    draftPairPrecedence.startTime = "9:00";
    draftPairPrecedence.endTime.reset();
    verifyInvalidValidation(
        draftPairPrecedence.validate(),
        "Calendar event draft start and end times must be both absent or both present."
        );

    using SeriesCase = std::pair<
        CalendarEventSeriesEditRequest,
        std::string_view
        >;
    std::vector<SeriesCase> seriesCases;
    const auto addSeriesCase = [&seriesCases](
        const auto& mutate,
        const std::string_view message
        )
    {
        auto request = validSeriesEditRequest();
        mutate(request);
        seriesCases.emplace_back(std::move(request), message);
    };
    addSeriesCase(
        [](auto& request) { request.editedStartDate = "2026-02-30"; },
        "Calendar repeat-series edit dates must be valid canonical ISO dates."
        );
    addSeriesCase(
        [](auto& request) { request.editedEndDate = "2026-09-21"; },
        "Calendar repeat-series edit end date must not precede its start date."
        );
    addSeriesCase(
        [](auto& request) { request.endTime.reset(); },
        "Calendar repeat-series edit times must be both absent or both present."
        );
    addSeriesCase(
        [](auto& request) { request.startTime = "9:00"; },
        "Calendar repeat-series edit times must be valid canonical 24-hour times."
        );
    addSeriesCase(
        [](auto& request)
        {
            request.allDay = true;
            request.timeStatus = "Unknown";
            request.startTime.reset();
            request.endTime.reset();
        },
        "All-day repeat-series edits require Timed status and no times."
        );
    addSeriesCase(
        [](auto& request)
        {
            request.startTime.reset();
            request.endTime.reset();
        },
        "Timed repeat-series edits require both start and end times."
        );
    addSeriesCase(
        [](auto& request)
        {
            request.editedEndDate = request.editedStartDate;
            request.endTime = "09:00";
        },
        "Calendar repeat-series edit end time must be after its start time."
        );
    addSeriesCase(
        [](auto& request) { request.timeStatus = "Unknown"; },
        "Unknown or unconfirmed repeat-series edits require no times."
        );
    for (const auto& [request, message] : seriesCases)
    {
        verifyInvalidValidation(request.validate(), message);
    }

    auto seriesTextPrecedence = validSeriesEditRequest();
    seriesTextPrecedence.title.clear();
    seriesTextPrecedence.startDate = "2026-02-30";
    verifyInvalidValidation(
        seriesTextPrecedence.validate(),
        "Calendar repeat-series edit text fields must be non-blank and bounded."
        );

    auto seriesSourceDatePrecedence = validSeriesEditRequest();
    seriesSourceDatePrecedence.startDate = "2026-02-30";
    seriesSourceDatePrecedence.editedStartDate = "2026-09-24";
    seriesSourceDatePrecedence.editedEndDate = "2026-09-23";
    seriesSourceDatePrecedence.endTime.reset();
    verifyInvalidValidation(
        seriesSourceDatePrecedence.validate(),
        "Calendar repeat-series edit dates must be valid canonical ISO dates."
        );

    auto seriesRangePrecedence = validSeriesEditRequest();
    seriesRangePrecedence.editedEndDate = "2026-09-21";
    seriesRangePrecedence.endTime.reset();
    verifyInvalidValidation(
        seriesRangePrecedence.validate(),
        "Calendar repeat-series edit end date must not precede its start date."
        );

    auto seriesPairPrecedence = validSeriesEditRequest();
    seriesPairPrecedence.startTime = "9:00";
    seriesPairPrecedence.endTime.reset();
    verifyInvalidValidation(
        seriesPairPrecedence.validate(),
        "Calendar repeat-series edit times must be both absent or both present."
        );
}

void NextApplicationCalendarEventTests::
calendarEventNameValidationUsesDomainClassifiersAndPreservesRawText()
{
    const auto acceptsTrimmedNamesAndPreservesInput = [](auto request)
    {
        request.eventType = " \tWorkshop \n";
        request.timeStatus = " \tTimed \n";
        if (!request.validate())
        {
            return false;
        }

        return request.eventType == " \tWorkshop \n"
            && request.timeStatus == " \tTimed \n";
    };

    const auto acceptsRawLimitsAndRejectsOverflow = [](
        auto request,
        const std::size_t eventTypeLimit,
        const std::size_t timeStatusLimit,
        const std::string_view errorMessage
        )
    {
        request.eventType = "Workshop";
        request.eventType.append(
            eventTypeLimit - request.eventType.size(),
            ' '
            );
        request.timeStatus = "Timed";
        request.timeStatus.append(
            timeStatusLimit - request.timeStatus.size(),
            ' '
            );
        if (request.eventType.size() != eventTypeLimit
            || request.timeStatus.size() != timeStatusLimit
            || !request.validate())
        {
            return false;
        }

        request.eventType.push_back(' ');
        auto oversizedEventType = request.validate();
        if (oversizedEventType
            || oversizedEventType.error().message != errorMessage)
        {
            return false;
        }

        request.eventType.pop_back();
        request.timeStatus.push_back(' ');
        const auto oversizedTimeStatus = request.validate();
        return !oversizedTimeStatus
            && oversizedTimeStatus.error().message == errorMessage;
    };

    const auto rejectsUnknownNamesWithFeatureMessage = [](
        auto request,
        const std::string_view errorMessage
        )
    {
        request.eventType = "Webinar";
        const auto invalidEventType = request.validate();
        if (invalidEventType
            || invalidEventType.error().message != errorMessage)
        {
            return false;
        }

        request.eventType = "Workshop";
        request.timeStatus = "Pending";
        const auto invalidTimeStatus = request.validate();
        return !invalidTimeStatus
            && invalidTimeStatus.error().message == errorMessage;
    };

    QVERIFY(acceptsTrimmedNamesAndPreservesInput(validSaveRequest()));
    QVERIFY(acceptsTrimmedNamesAndPreservesInput(validEditDraft()));
    QVERIFY(acceptsTrimmedNamesAndPreservesInput(validSeriesEditRequest()));

    QVERIFY(acceptsRawLimitsAndRejectsOverflow(
        validSaveRequest(),
        kCalendarEventSaveMaxEventTypeLength,
        kCalendarEventSaveMaxTimeStatusLength,
        "Calendar event text fields must be non-blank and bounded."
        ));
    QVERIFY(acceptsRawLimitsAndRejectsOverflow(
        validEditDraft(),
        kCalendarEventEditDraftMaxEventTypeLength,
        kCalendarEventEditDraftMaxTimeStatusLength,
        "Calendar event draft text fields must be non-blank and bounded."
        ));
    QVERIFY(acceptsRawLimitsAndRejectsOverflow(
        validSeriesEditRequest(),
        kCalendarEventSeriesEditMaxEventTypeLength,
        kCalendarEventSeriesEditMaxTimeStatusLength,
        "Calendar repeat-series edit text fields must be non-blank and bounded."
        ));

    QVERIFY(rejectsUnknownNamesWithFeatureMessage(
        validSaveRequest(),
        "Calendar event text fields must be non-blank and bounded."
        ));
    QVERIFY(rejectsUnknownNamesWithFeatureMessage(
        validEditDraft(),
        "Calendar event draft text fields must be non-blank and bounded."
        ));
    QVERIFY(rejectsUnknownNamesWithFeatureMessage(
        validSeriesEditRequest(),
        "Calendar repeat-series edit text fields must be non-blank and bounded."
        ));
}

void NextApplicationCalendarEventTests::
timingAcceptsFullGregorianRangeAndCrossDayClocks()
{
    auto save = validSaveRequest();
    save.startDate = "0001-01-01";
    save.endDate = "9999-12-31";
    save.startTime = "23:59";
    save.endTime = "00:00";
    save.timeStatus = " \tTimed \n";
    QVERIFY(save.validate());

    auto draft = validEditDraft();
    draft.startDate = "0001-01-01";
    draft.endDate = "9999-12-31";
    draft.startTime = "23:59";
    draft.endTime = "00:00";
    draft.timeStatus = " \tTimed \n";
    QVERIFY(draft.validate());

    auto series = validSeriesEditRequest();
    series.startDate = "0001-01-01";
    series.editedStartDate = "0001-01-01";
    series.editedEndDate = "9999-12-31";
    series.startTime = "23:59";
    series.endTime = "00:00";
    series.timeStatus = " \tTimed \n";
    QVERIFY(series.validate());
}

void NextApplicationCalendarEventTests::
saveRequestContractHasNoQtOrLegacySurface()
{
    using Port = CalendarEventSavePort;
    using SaveResult = decltype(
        std::declval<Port&>().saveEvent(
            std::declval<const CalendarEventSaveRequest&>()
            )
        );

    static_assert(std::is_same_v<
        SaveResult,
        CalendarEventSaveResult
        >);
    static_assert(std::is_same_v<
        CalendarEventSaveResult,
        Domain::Result<CalendarEventId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSaveRequest>().id),
        std::optional<CalendarEventId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSaveRequest>().startTime),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSaveRequest>().endTime),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSaveRequest>().allDay),
        bool
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<CalendarEventSaveRequest>().title)
        >);
    static_assert(!std::is_copy_constructible_v<Port>);

    QVERIFY(true);
}

void NextApplicationCalendarEventTests::
importSaveRequestBoundsOrderedCreateBatchAndNoOp()
{
    CalendarEventImportSaveRequest emptyRequest;
    QVERIFY(emptyRequest.validate());

    CalendarEventImportSaveRequest request;
    request.events.reserve(kCalendarEventImportSaveMaxEvents);
    for (std::size_t index = 0;
         index < kCalendarEventImportSaveMaxEvents;
         ++index)
    {
        CalendarEventSaveRequest event = validSaveRequest();
        event.title = "Imported event " + std::to_string(index);
        request.events.push_back(std::move(event));
    }

    QVERIFY(request.validate());
    QCOMPARE(request.events.size(), kCalendarEventImportSaveMaxEvents);

    request.events.push_back(validSaveRequest());
    const auto oversized = request.validate();
    QVERIFY(!oversized);
    QCOMPARE(oversized.error().code, ErrorCode::InvalidInput);
}

void NextApplicationCalendarEventTests::
importSaveRequestRejectsUpdatesAndInvalidEvents()
{
    CalendarEventImportSaveRequest request;
    request.events.push_back(validSaveRequest());
    QVERIFY(request.validate());

    request.events.front().id = calendarEventId("event-1");
    verifyInvalidValidation(request.validate());

    request.events.front().id.reset();
    request.events.front().timeStatus = "Unknown";
    request.events.front().startTime = "09:00";
    request.events.front().endTime = "10:00";
    verifyInvalidValidation(request.validate());
}

void NextApplicationCalendarEventTests::
importSaveRequestContractHasNoQtOrLegacySurface()
{
    using Port = CalendarEventImportSavePort;
    using SaveResult = decltype(
        std::declval<Port&>().saveImportedEvents(
            std::declval<const CalendarEventImportSaveRequest&>()
            )
        );

    static_assert(std::is_same_v<
        SaveResult,
        CalendarEventImportSaveResult
        >);
    static_assert(std::is_same_v<
        CalendarEventImportSaveResult,
        Domain::Result<std::vector<CalendarEventId>>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventImportSaveRequest>().events),
        std::vector<CalendarEventSaveRequest>
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<CalendarEventImportSaveRequest>().events)
        >);
    static_assert(!std::is_copy_constructible_v<Port>);

    QVERIFY(true);
}

void NextApplicationCalendarEventTests::
deleteAllPortContractUsesStructuredQtFreeResult()
{
    using Port = CalendarEventDeleteAllPort;
    using DeleteResult = decltype(
        std::declval<Port&>().deleteAllEvents()
        );

    static_assert(std::is_same_v<
        DeleteResult,
        CalendarEventDeleteAllResult
        >);
    static_assert(std::is_same_v<
        CalendarEventDeleteAllResult,
        Domain::Result<void>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const Port&>().isAvailable()),
        bool
        >);
    static_assert(!std::is_copy_constructible_v<Port>);

    QVERIFY(true);
}

void NextApplicationCalendarEventTests::
deleteAllUseCaseForwardsAvailability()
{
    FakeCalendarEventDeleteAllPort port;

    QVERIFY(CalendarEventDeleteAllUseCase::isAvailable(port));
    QCOMPARE(port.availabilityCalls, 1);
    QCOMPARE(port.deleteCalls, 0);

    port.available = false;
    QVERIFY(!CalendarEventDeleteAllUseCase::isAvailable(port));
    QCOMPARE(port.availabilityCalls, 2);
    QCOMPARE(port.deleteCalls, 0);
}

void NextApplicationCalendarEventTests::
deleteAllUseCaseDeletesOnceAndReturnsSuccess()
{
    using UseCaseResult = decltype(
        CalendarEventDeleteAllUseCase::execute(
            std::declval<CalendarEventDeleteAllPort&>()
            )
        );
    static_assert(std::is_same_v<
        UseCaseResult,
        CalendarEventDeleteAllResult
        >);

    FakeCalendarEventDeleteAllPort port;
    const CalendarEventDeleteAllResult result =
        CalendarEventDeleteAllUseCase::execute(port);

    QVERIFY(result);
    QCOMPARE(port.deleteCalls, 1);
    QCOMPARE(port.availabilityCalls, 0);
}

void NextApplicationCalendarEventTests::
deleteAllUseCasePropagatesPortFailure()
{
    FakeCalendarEventDeleteAllPort port;
    port.response = CalendarEventDeleteAllResult::failure({
        .code = ErrorCode::Technical,
        .message = "Calendar events could not be reset.",
        .recoverable = false
    });

    const CalendarEventDeleteAllResult result =
        CalendarEventDeleteAllUseCase::execute(port);

    QVERIFY(!result);
    QCOMPARE(port.deleteCalls, 1);
    QCOMPARE(port.availabilityCalls, 0);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("Calendar events could not be reset.")
        );
    QVERIFY(!result.error().recoverable);
}

void NextApplicationCalendarEventTests::
editDraftBoundsAndTimeStatusPolicyIsExplicit()
{
    auto draft = validEditDraft();
    QVERIFY(draft.validate());
    QVERIFY(validateCalendarEventEditDraft(draft));

    auto blankId = draft;
    blankId.id = calendarEventId(" \t");
    QVERIFY(!blankId.validate());

    auto oversizedId = draft;
    oversizedId.id = calendarEventId(
        std::string(kCalendarEventEditDraftMaxIdentifierLength + 1, 'i')
        );
    QVERIFY(!oversizedId.validate());

    auto blankSeries = draft;
    blankSeries.repeatSeriesId = " \t";
    QVERIFY(!blankSeries.validate());

    auto oversizedSeries = draft;
    oversizedSeries.repeatSeriesId = std::string(
        kCalendarEventEditDraftMaxRepeatSeriesIdLength + 1,
        'r'
        );
    QVERIFY(!oversizedSeries.validate());

    auto exactTitle = draft;
    exactTitle.title = std::string(
        kCalendarEventEditDraftMaxTitleLength,
        't'
        );
    QVERIFY(exactTitle.validate());

    auto oversizedTitle = exactTitle;
    oversizedTitle.title.push_back('x');
    QVERIFY(!oversizedTitle.validate());

    auto invalidDate = draft;
    invalidDate.startDate = "2026-02-30";
    QVERIFY(!invalidDate.validate());

    auto invalidTime = draft;
    invalidTime.startTime = "9:00";
    invalidTime.endTime = "10:00";
    QVERIFY(!invalidTime.validate());

    auto allDay = draft;
    allDay.allDay = true;
    allDay.startTime.reset();
    allDay.endTime.reset();
    QVERIFY(allDay.validate());

    auto allDayWithTime = allDay;
    allDayWithTime.startTime = "09:00";
    allDayWithTime.endTime = "10:00";
    QVERIFY(!allDayWithTime.validate());

    auto unconfirmed = draft;
    unconfirmed.timeStatus = "Unconfirmed";
    unconfirmed.startTime.reset();
    unconfirmed.endTime.reset();
    QVERIFY(unconfirmed.validate());

    auto unconfirmedWithTime = unconfirmed;
    unconfirmedWithTime.startTime = "09:00";
    unconfirmedWithTime.endTime = "10:00";
    QVERIFY(!unconfirmedWithTime.validate());
}

void NextApplicationCalendarEventTests::
editDraftIsCopyableEqualAndIndependentlyReleasable()
{
    static_assert(std::is_copy_constructible_v<CalendarEventEditDraft>);
    static_assert(std::is_copy_assignable_v<CalendarEventEditDraft>);
    static_assert(std::is_move_constructible_v<CalendarEventEditDraft>);
    static_assert(std::is_move_assignable_v<CalendarEventEditDraft>);

    const auto draft = validEditDraft();
    const auto copy = draft;
    QVERIFY(copy == draft);

    auto changed = copy;
    changed.title = "Changed title";
    QVERIFY(changed != draft);

    CalendarEventEditDraft assigned;
    assigned = draft;
    QVERIFY(assigned == draft);

    const auto moved = std::move(assigned);
    QVERIFY(moved == draft);
}

void NextApplicationCalendarEventTests::
editDraftContractHasNoQtOrLegacySurface()
{
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventEditDraft>().id),
        std::optional<CalendarEventId>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventEditDraft>().repeatSeriesId),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventEditDraft>().startDate),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventEditDraft>().endDate),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventEditDraft>().startTime),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventEditDraft>().endTime),
        std::optional<std::string>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventEditDraft>().allDay),
        bool
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<CalendarEventEditDraft>().title)
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<CalendarEventEditDraft>().eventType)
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<CalendarEventEditDraft>().timeStatus)
        >);

    QVERIFY(true);
}

void NextApplicationCalendarEventTests::
seriesCreateRequestBoundsAndOccurrencesRemainTyped()
{
    auto request = validSeriesCreateRequest();
    QVERIFY(request.validate());
    QVERIFY(validateCalendarEventSeriesCreateRequest(request));
    QCOMPARE(request.occurrences.size(), std::size_t(1));

    auto blankSeries = request;
    blankSeries.repeatSeriesId = " \t";
    QVERIFY(!blankSeries.validate());
    QCOMPARE(blankSeries.validate().error().code, ErrorCode::InvalidInput);

    auto oversizedSeries = request;
    oversizedSeries.repeatSeriesId = std::string(
        kCalendarEventSeriesCreateMaxRepeatSeriesIdLength + 1,
        'r'
        );
    QVERIFY(!oversizedSeries.validate());

    auto emptySeries = request;
    emptySeries.occurrences.clear();
    QVERIFY(!emptySeries.validate());

    auto exactCapacity = request;
    exactCapacity.occurrences.assign(
        kCalendarEventSeriesCreateMaxOccurrences,
        validSaveRequest()
        );
    QVERIFY(exactCapacity.validate());

    auto oversizedOccurrences = exactCapacity;
    oversizedOccurrences.occurrences.push_back(validSaveRequest());
    QVERIFY(!oversizedOccurrences.validate());

    auto invalidOccurrence = request;
    invalidOccurrence.occurrences.front().startDate = "2026-02-30";
    QVERIFY(!invalidOccurrence.validate());
    QCOMPARE(
        invalidOccurrence.validate().error().code,
        ErrorCode::InvalidInput
        );
}

void NextApplicationCalendarEventTests::
seriesCreateRequestContractHasNoQtOrLegacySurface()
{
    using Port = CalendarEventSeriesCreatePort;
    using CreateResult = decltype(
        std::declval<Port&>().createRepeatSeries(
            std::declval<const CalendarEventSeriesCreateRequest&>()
            )
        );

    static_assert(std::is_same_v<
        CreateResult,
        CalendarEventSeriesCreateResult
        >);
    static_assert(std::is_same_v<
        CalendarEventSeriesCreateResult,
        Domain::Result<std::vector<CalendarEventId>>
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
        decltype(std::declval<CalendarEventSeriesCreateRequest>()
                     .occurrences.front().allDay),
        bool
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<CalendarEventSeriesCreateRequest>()
                     .occurrences)
        >);
    static_assert(!std::is_copy_constructible_v<Port>);

    QVERIFY(true);
}

void NextApplicationCalendarEventTests::
repeatOccurrencePlanSupportsDailyWeeklyAndInclusiveUntil()
{
    auto seed = validSaveRequest();
    seed.startDate = "2026-09-20";
    seed.endDate = seed.startDate;

    const auto daily = planCalendarEventRepeatOccurrences(
        seed,
        "daily-series",
        CalendarEventRepeatFrequency::Daily,
        "2026-09-22"
        );
    QVERIFY(daily);
    QCOMPARE(daily.value().repeatSeriesId, std::string("daily-series"));
    verifyOccurrenceStartDates(
        daily.value(),
        {"2026-09-20", "2026-09-21", "2026-09-22"}
        );

    const auto weekly = planCalendarEventRepeatOccurrences(
        seed,
        "weekly-series",
        CalendarEventRepeatFrequency::Weekly,
        "2026-10-11"
        );
    QVERIFY(weekly);
    verifyOccurrenceStartDates(
        weekly.value(),
        {"2026-09-20", "2026-09-27", "2026-10-04", "2026-10-11"}
        );

    const auto untilStart = planCalendarEventRepeatOccurrences(
        seed,
        "inclusive-start",
        CalendarEventRepeatFrequency::Daily,
        seed.startDate
        );
    QVERIFY(untilStart);
    QCOMPARE(untilStart.value().occurrences.size(), std::size_t(1));
    QCOMPARE(
        untilStart.value().occurrences.front().startDate,
        seed.startDate
        );

    auto lowerBoundarySeed = validSaveRequest();
    lowerBoundarySeed.startDate = "0001-01-01";
    lowerBoundarySeed.endDate = lowerBoundarySeed.startDate;
    const auto lowerBoundary = planCalendarEventRepeatOccurrences(
        lowerBoundarySeed,
        "lower-date-boundary",
        CalendarEventRepeatFrequency::Daily,
        "0001-01-01"
        );
    QVERIFY(lowerBoundary);
    QCOMPARE(
        lowerBoundary.value().occurrences.front().startDate,
        std::string("0001-01-01")
        );

    auto upperBoundarySeed = validSaveRequest();
    upperBoundarySeed.startDate = "9999-12-31";
    upperBoundarySeed.endDate = upperBoundarySeed.startDate;
    const auto upperBoundary = planCalendarEventRepeatOccurrences(
        upperBoundarySeed,
        "upper-date-boundary",
        CalendarEventRepeatFrequency::Daily,
        "9999-12-31"
        );
    QVERIFY(upperBoundary);
    QCOMPARE(
        upperBoundary.value().occurrences.front().startDate,
        std::string("9999-12-31")
        );
}

void NextApplicationCalendarEventTests::
repeatOccurrencePlanClampsMonthlyDatesFromPreviousOccurrence()
{
    auto nonLeapSeed = validSaveRequest();
    nonLeapSeed.startDate = "2025-01-31";
    nonLeapSeed.endDate = nonLeapSeed.startDate;
    const auto nonLeap = planCalendarEventRepeatOccurrences(
        nonLeapSeed,
        "non-leap-months",
        CalendarEventRepeatFrequency::Monthly,
        "2025-04-30"
        );
    QVERIFY(nonLeap);
    verifyOccurrenceStartDates(
        nonLeap.value(),
        {"2025-01-31", "2025-02-28", "2025-03-28", "2025-04-28"}
        );

    auto leapSeed = validSaveRequest();
    leapSeed.startDate = "2024-01-31";
    leapSeed.endDate = leapSeed.startDate;
    const auto leap = planCalendarEventRepeatOccurrences(
        leapSeed,
        "leap-months",
        CalendarEventRepeatFrequency::Monthly,
        "2024-04-30"
        );
    QVERIFY(leap);
    verifyOccurrenceStartDates(
        leap.value(),
        {"2024-01-31", "2024-02-29", "2024-03-29", "2024-04-29"}
        );
}

void NextApplicationCalendarEventTests::
repeatOccurrencePlanPreservesSeedFieldsAndDateDuration()
{
    auto seed = validSaveRequest();
    seed.id = calendarEventId("existing-seed-event");
    seed.title = "Workshop series title";
    seed.startDate = "2026-09-20";
    seed.endDate = "2026-09-22";
    seed.startTime = "14:15";
    seed.endTime = "15:45";
    seed.allDay = false;
    seed.eventType = "Workshop";
    seed.timeStatus = "Timed";

    const auto plan = planCalendarEventRepeatOccurrences(
        seed,
        "caller-generated-series-id",
        CalendarEventRepeatFrequency::Daily,
        "2026-09-21"
        );
    QVERIFY(plan);
    QCOMPARE(
        plan.value().repeatSeriesId,
        std::string("caller-generated-series-id")
        );
    QCOMPARE(plan.value().occurrences.size(), std::size_t(2));

    const auto& first = plan.value().occurrences.at(0);
    const auto& second = plan.value().occurrences.at(1);
    QVERIFY(!first.id.has_value());
    QVERIFY(!second.id.has_value());
    QCOMPARE(first.title, seed.title);
    QCOMPARE(second.title, seed.title);
    QCOMPARE(first.startDate, std::string("2026-09-20"));
    QCOMPARE(first.endDate, std::string("2026-09-22"));
    QCOMPARE(second.startDate, std::string("2026-09-21"));
    QCOMPARE(second.endDate, std::string("2026-09-23"));
    QCOMPARE(first.startTime, seed.startTime);
    QCOMPARE(second.startTime, seed.startTime);
    QCOMPARE(first.endTime, seed.endTime);
    QCOMPARE(second.endTime, seed.endTime);
    QCOMPARE(first.allDay, seed.allDay);
    QCOMPARE(second.allDay, seed.allDay);
    QCOMPARE(first.eventType, seed.eventType);
    QCOMPARE(second.eventType, seed.eventType);
    QCOMPARE(first.timeStatus, seed.timeStatus);
    QCOMPARE(second.timeStatus, seed.timeStatus);
}

void NextApplicationCalendarEventTests::
repeatOccurrencePlanRejectsInvalidSeedRangeUntilAndFrequency()
{
    auto invalidSeed = validSaveRequest();
    invalidSeed.startDate = "2026-02-30";
    verifyInvalidRepeatPlan(planCalendarEventRepeatOccurrences(
        invalidSeed,
        "invalid-seed",
        CalendarEventRepeatFrequency::Daily,
        "2026-09-22"
        ));

    auto reversedSeedRange = validSaveRequest();
    reversedSeedRange.startDate = "2026-09-20";
    reversedSeedRange.endDate = "2026-09-19";
    verifyInvalidRepeatPlan(planCalendarEventRepeatOccurrences(
        reversedSeedRange,
        "reversed-seed-range",
        CalendarEventRepeatFrequency::Daily,
        "2026-09-22"
        ));

    const auto validSeed = validSaveRequest();
    verifyInvalidRepeatPlan(planCalendarEventRepeatOccurrences(
        validSeed,
        "invalid-until",
        CalendarEventRepeatFrequency::Daily,
        "2026-02-30"
        ));
    verifyInvalidRepeatPlan(planCalendarEventRepeatOccurrences(
        validSeed,
        "until-before-start",
        CalendarEventRepeatFrequency::Daily,
        "2026-09-19"
        ));
    verifyInvalidRepeatPlan(planCalendarEventRepeatOccurrences(
        validSeed,
        "invalid-frequency",
        static_cast<CalendarEventRepeatFrequency>(99),
        "2026-09-22"
        ));

    auto endOfRangeSeed = validSaveRequest();
    endOfRangeSeed.startDate = "9999-12-30";
    endOfRangeSeed.endDate = "9999-12-31";
    verifyInvalidRepeatPlan(planCalendarEventRepeatOccurrences(
        endOfRangeSeed,
        "end-of-range",
        CalendarEventRepeatFrequency::Daily,
        "9999-12-31"
        ));
}

void NextApplicationCalendarEventTests::
repeatOccurrencePlanAccepts366AndRejects367Occurrences()
{
    auto seed = validSaveRequest();
    seed.startDate = "2023-01-01";
    seed.endDate = seed.startDate;
    const auto exactLimit = planCalendarEventRepeatOccurrences(
        seed,
        "exact-limit",
        CalendarEventRepeatFrequency::Daily,
        "2024-01-01"
        );
    QVERIFY(exactLimit);
    QCOMPARE(
        exactLimit.value().occurrences.size(),
        kCalendarEventSeriesCreateMaxOccurrences
        );
    QCOMPARE(
        exactLimit.value().occurrences.back().startDate,
        std::string("2024-01-01")
        );

    seed.startDate = "2024-01-01";
    seed.endDate = seed.startDate;
    const auto overLimit = planCalendarEventRepeatOccurrences(
        seed,
        "over-limit",
        CalendarEventRepeatFrequency::Daily,
        "2025-01-01"
        );
    verifyInvalidRepeatPlan(overLimit);
}

void NextApplicationCalendarEventTests::
repeatOccurrencePlanHasTypedQtFreeContract()
{
    using PlanResult = decltype(planCalendarEventRepeatOccurrences(
        std::declval<const CalendarEventSaveRequest&>(),
        std::declval<std::string>(),
        std::declval<CalendarEventRepeatFrequency>(),
        std::declval<std::string_view>()
        ));
    static_assert(std::is_same_v<
        PlanResult,
        Result<CalendarEventSeriesCreateRequest>
        >);
    static_assert(std::is_enum_v<CalendarEventRepeatFrequency>);
    QVERIFY(true);
}

void NextApplicationCalendarEventTests::
repeatSeriesCreateUseCasePlansAndForwardsExactlyOnce()
{
    using UseCaseResult = decltype(
        CalendarEventSeriesCreateUseCase::execute(
            std::declval<CalendarEventSeriesCreatePort&>(),
            std::declval<const CalendarEventSaveRequest&>(),
            std::declval<std::string>(),
            std::declval<CalendarEventRepeatFrequency>(),
            std::declval<std::string_view>()
            )
        );
    static_assert(std::is_same_v<
        UseCaseResult,
        CalendarEventSeriesCreateResult
        >);

    CalendarEventSaveRequest seed = validSaveRequest();
    seed.id = calendarEventId("event-existing");
    seed.title = "Repeat workshop";
    seed.startDate = "2026-09-20";
    seed.endDate = "2026-09-21";
    seed.startTime = "08:30";
    seed.endTime = "10:15";
    seed.eventType = "Workshop";
    seed.timeStatus = "Timed";

    FakeCalendarEventSeriesCreatePort port;
    const CalendarEventId firstId = calendarEventId("event-101");
    const CalendarEventId secondId = calendarEventId("event-102");
    port.response = CalendarEventSeriesCreateResult::success({
        firstId,
        secondId
    });

    const CalendarEventSeriesCreateResult result =
        CalendarEventSeriesCreateUseCase::execute(
            port,
            seed,
            "generated-series-123",
            CalendarEventRepeatFrequency::Daily,
            "2026-09-21"
            );

    QVERIFY(result);
    QCOMPARE(port.createCalls, 1);
    QVERIFY(port.receivedRequest.has_value());

    CalendarEventSaveRequest firstOccurrence = seed;
    firstOccurrence.id.reset();
    firstOccurrence.startDate = "2026-09-20";
    firstOccurrence.endDate = "2026-09-21";
    CalendarEventSaveRequest secondOccurrence = seed;
    secondOccurrence.id.reset();
    secondOccurrence.startDate = "2026-09-21";
    secondOccurrence.endDate = "2026-09-22";

    const CalendarEventSeriesCreateRequest expectedRequest{
        "generated-series-123",
        {firstOccurrence, secondOccurrence}
    };
    QVERIFY(*port.receivedRequest == expectedRequest);
    QCOMPARE(result.value().size(), std::size_t(2));
    QVERIFY(result.value().at(0) == firstId);
    QVERIFY(result.value().at(1) == secondId);
}

void NextApplicationCalendarEventTests::
repeatSeriesCreateUseCasePropagatesPortFailure()
{
    FakeCalendarEventSeriesCreatePort port;
    port.response = CalendarEventSeriesCreateResult::failure({
        .code = ErrorCode::Conflict,
        .message = "The repeat series could not be stored.",
        .recoverable = true
    });

    const CalendarEventSeriesCreateResult result =
        CalendarEventSeriesCreateUseCase::execute(
            port,
            validSaveRequest(),
            "series-with-failure",
            CalendarEventRepeatFrequency::Weekly,
            "2026-09-20"
            );

    QVERIFY(!result);
    QCOMPARE(port.createCalls, 1);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QCOMPARE(
        result.error().message,
        std::string("The repeat series could not be stored.")
        );
    QVERIFY(result.error().recoverable);
}

void NextApplicationCalendarEventTests::
repeatSeriesCreateUseCaseRejectsInvalidPlanWithoutCallingPort()
{
    FakeCalendarEventSeriesCreatePort port;

    const CalendarEventSeriesCreateResult result =
        CalendarEventSeriesCreateUseCase::execute(
            port,
            validSaveRequest(),
            "series-with-invalid-range",
            CalendarEventRepeatFrequency::Daily,
            "2026-09-19"
            );

    QVERIFY(!result);
    QCOMPARE(port.createCalls, 0);
    QVERIFY(!port.receivedRequest.has_value());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(
        result.error().message,
        std::string(
            "Calendar repeat until date must not precede the seed start date."
            )
        );
    QVERIFY(!result.error().recoverable);
}

void NextApplicationCalendarEventTests::
seriesDeleteRequestValidationIsCanonicalAndQtFree()
{
    using ValidationResult = decltype(
        std::declval<const CalendarEventSeriesDeleteRequest&>().validate()
        );
    static_assert(std::is_same_v<ValidationResult, Result<void>>);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesDeleteRequest>().repeatSeriesId),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventSeriesDeleteRequest>().startDate),
        std::string
        >);

    const CalendarEventSeriesDeleteRequest leapDayRequest{
        "series-delete-leap-day",
        "2024-02-29"
    };
    QVERIFY(leapDayRequest.validate());
    const CalendarEventSeriesDeleteRequest maximumIdRequest{
        std::string(kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength, 'r'),
        "2026-12-08"
    };
    QVERIFY(maximumIdRequest.validate());

    const std::string diagnostic =
        "Calendar repeat-series delete request must contain a non-blank "
        "bounded series identifier and a valid ISO start date.";
    const std::vector<CalendarEventSeriesDeleteRequest> invalidRequests{
        {" \t", "2026-12-08"},
        {
            std::string(
                kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength + 1,
                'r'
                ),
            "2026-12-08"
        },
        {"series-delete-malformed-date", "2026-2-08"},
        {"series-delete-impossible-date", "2026-02-30"}
    };
    for (const CalendarEventSeriesDeleteRequest& request : invalidRequests)
    {
        const Result<void> validation = request.validate();
        QVERIFY(!validation);
        QCOMPARE(validation.error().code, ErrorCode::InvalidInput);
        QCOMPARE(validation.error().message, diagnostic);
        QVERIFY(!validation.error().recoverable);
    }
}

void NextApplicationCalendarEventTests::
repeatSeriesDeleteUseCaseForwardsRequestAndResultOnce()
{
    using UseCaseResult = decltype(
        CalendarEventSeriesDeleteUseCase::execute(
            std::declval<CalendarEventSeriesDeletePort&>(),
            std::declval<const CalendarEventSeriesDeleteRequest&>()
            )
        );
    static_assert(std::is_same_v<UseCaseResult, Result<void>>);

    const CalendarEventSeriesDeleteRequest request{
        " \tseries-delete-original-bytes \n",
        "2024-02-29"
    };
    FakeCalendarEventSeriesDeletePort port;
    const CalendarEventSeriesDeleteResult result =
        CalendarEventSeriesDeleteUseCase::execute(port, request);

    QVERIFY(result);
    QCOMPARE(port.deleteCalls, 1);
    QVERIFY(port.receivedRequest.has_value());
    QVERIFY(*port.receivedRequest == request);
}

void NextApplicationCalendarEventTests::
repeatSeriesDeleteUseCasePropagatesPortFailure()
{
    FakeCalendarEventSeriesDeletePort port;
    port.response = CalendarEventSeriesDeleteResult::failure({
        .code = ErrorCode::Conflict,
        .message = "The selected repeat-series suffix could not be deleted.",
        .recoverable = true
    });

    const CalendarEventSeriesDeleteResult result =
        CalendarEventSeriesDeleteUseCase::execute(
            port,
            {"series-delete-failure", "2026-12-08"}
            );

    QVERIFY(!result);
    QCOMPARE(port.deleteCalls, 1);
    QVERIFY(port.receivedRequest.has_value());
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QCOMPARE(
        result.error().message,
        std::string("The selected repeat-series suffix could not be deleted.")
        );
    QVERIFY(result.error().recoverable);
}

void NextApplicationCalendarEventTests::
repeatSeriesDeleteUseCaseRejectsInvalidRequestWithoutCallingPort()
{
    FakeCalendarEventSeriesDeletePort port;
    const std::string diagnostic =
        "Calendar repeat-series delete request must contain a non-blank "
        "bounded series identifier and a valid ISO start date.";
    const std::vector<CalendarEventSeriesDeleteRequest> invalidRequests{
        {"", "2026-12-08"},
        {" \t", "2026-12-08"},
        {
            std::string(
                kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength + 1,
                'r'
                ),
            "2026-12-08"
        },
        {"series-delete-malformed-date", "2026/12/08"},
        {"series-delete-impossible-date", "2026-04-31"}
    };

    for (const CalendarEventSeriesDeleteRequest& request : invalidRequests)
    {
        const CalendarEventSeriesDeleteResult result =
            CalendarEventSeriesDeleteUseCase::execute(port, request);

        QVERIFY(!result);
        QCOMPARE(port.deleteCalls, 0);
        QVERIFY(!port.receivedRequest.has_value());
        QCOMPARE(result.error().code, ErrorCode::InvalidInput);
        QCOMPARE(result.error().message, diagnostic);
        QVERIFY(!result.error().recoverable);
    }
}

void NextApplicationCalendarEventTests::
singleEventDeleteUseCaseForwardsIdAndResultOnce()
{
    using UseCaseResult = decltype(
        CalendarEventDeleteUseCase::execute(
            std::declval<CalendarEventDeletePort&>(),
            std::declval<const CalendarEventId&>()
            )
        );
    static_assert(std::is_same_v<UseCaseResult, Result<void>>);

    const CalendarEventId requestedId = calendarEventId("0042");
    FakeCalendarEventDeletePort port;
    const CalendarEventDeleteResult result =
        CalendarEventDeleteUseCase::execute(port, requestedId);

    QVERIFY(result);
    QCOMPARE(port.deleteCalls, 1);
    QVERIFY(port.receivedEventId.has_value());
    QCOMPARE(*port.receivedEventId, requestedId);
}

void NextApplicationCalendarEventTests::
singleEventDeleteUseCasePropagatesPortFailure()
{
    FakeCalendarEventDeletePort port;
    port.response = CalendarEventDeleteResult::failure({
        .code = ErrorCode::Technical,
        .message = "Calendar event could not be deleted.",
        .recoverable = false
    });

    const CalendarEventId requestedId = calendarEventId("event-delete-failure");
    const CalendarEventDeleteResult result =
        CalendarEventDeleteUseCase::execute(port, requestedId);

    QVERIFY(!result);
    QCOMPARE(port.deleteCalls, 1);
    QVERIFY(port.receivedEventId.has_value());
    QCOMPARE(*port.receivedEventId, requestedId);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(
        result.error().message,
        std::string("Calendar event could not be deleted.")
        );
    QVERIFY(!result.error().recoverable);
}

void NextApplicationCalendarEventTests::
seriesEditRequestBoundsAndDatesRemainTyped()
{
    auto request = validSeriesEditRequest();
    QVERIFY(request.validate());
    QVERIFY(validateCalendarEventSeriesEditRequest(request));

    auto blankSeries = request;
    blankSeries.repeatSeriesId = " \t";
    QVERIFY(!blankSeries.validate());

    auto oversizedSeries = request;
    oversizedSeries.repeatSeriesId = std::string(
        kCalendarEventSeriesEditMaxRepeatSeriesIdLength + 1,
        'r'
        );
    QVERIFY(!oversizedSeries.validate());

    auto exactTitle = request;
    exactTitle.title = std::string(
        kCalendarEventSeriesEditMaxTitleLength,
        't'
        );
    QVERIFY(exactTitle.validate());

    auto oversizedTitle = exactTitle;
    oversizedTitle.title.push_back('x');
    QVERIFY(!oversizedTitle.validate());

    auto invalidSourceDate = request;
    invalidSourceDate.startDate = "2026-02-30";
    QVERIFY(!invalidSourceDate.validate());

    auto invalidEditedDate = request;
    invalidEditedDate.editedStartDate = "2026-13-01";
    QVERIFY(!invalidEditedDate.validate());

    auto reversedEditedDates = request;
    reversedEditedDates.editedEndDate = "2026-09-21";
    QVERIFY(!reversedEditedDates.validate());

    auto oversizedEventType = request;
    oversizedEventType.eventType = std::string(
        kCalendarEventSeriesEditMaxEventTypeLength + 1,
        'e'
        );
    QVERIFY(!oversizedEventType.validate());

    auto oversizedTimeStatus = request;
    oversizedTimeStatus.timeStatus = std::string(
        kCalendarEventSeriesEditMaxTimeStatusLength + 1,
        's'
        );
    QVERIFY(!oversizedTimeStatus.validate());

    auto invalidTime = request;
    invalidTime.startTime = "9:00";
    invalidTime.endTime = "10:00";
    QVERIFY(!invalidTime.validate());
}

void NextApplicationCalendarEventTests::
seriesEditRequestAllDayAndTimeStatusPolicyIsExplicit()
{
    auto allDay = validSeriesEditRequest();
    allDay.allDay = true;
    allDay.timeStatus = "Timed";
    allDay.startTime.reset();
    allDay.endTime.reset();
    QVERIFY(allDay.validate());

    auto allDayWithTime = allDay;
    allDayWithTime.startTime = "09:00";
    allDayWithTime.endTime = "10:00";
    QVERIFY(!allDayWithTime.validate());

    auto allDayUnknown = allDay;
    allDayUnknown.timeStatus = "Unknown";
    QVERIFY(!allDayUnknown.validate());

    auto timedWithoutTimes = validSeriesEditRequest();
    timedWithoutTimes.startTime.reset();
    timedWithoutTimes.endTime.reset();
    QVERIFY(!timedWithoutTimes.validate());

    auto partialTimedRange = validSeriesEditRequest();
    partialTimedRange.endTime.reset();
    QVERIFY(!partialTimedRange.validate());

    auto reversedTimedRange = validSeriesEditRequest();
    reversedTimedRange.editedEndDate = reversedTimedRange.editedStartDate;
    reversedTimedRange.endTime = "08:00";
    QVERIFY(!reversedTimedRange.validate());

    auto unknownTime = validSeriesEditRequest();
    unknownTime.timeStatus = "Unknown";
    unknownTime.startTime.reset();
    unknownTime.endTime.reset();
    QVERIFY(unknownTime.validate());

    auto unconfirmedTime = unknownTime;
    unconfirmedTime.timeStatus = "Unconfirmed";
    QVERIFY(unconfirmedTime.validate());

    auto unknownWithTime = unknownTime;
    unknownWithTime.startTime = "09:00";
    unknownWithTime.endTime = "10:00";
    QVERIFY(!unknownWithTime.validate());
}

void NextApplicationCalendarEventTests::
seriesEditRequestContractHasNoQtOrLegacySurface()
{
    using Port = CalendarEventSeriesEditPort;
    using EditResult = decltype(
        std::declval<Port&>().editRepeatSeriesFromDate(
            std::declval<const CalendarEventSeriesEditRequest&>()
            )
        );

    static_assert(std::is_same_v<
        EditResult,
        CalendarEventSeriesEditResult
        >);
    static_assert(std::is_same_v<
        CalendarEventSeriesEditResult,
        Domain::Result<void>
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
    static_assert(!std::is_pointer_v<
        decltype(std::declval<CalendarEventSeriesEditRequest>().title)
        >);

    QVERIFY(true);
}

void NextApplicationCalendarEventTests::
seriesEditPlanPreservesOrderAndPropagatesRequestFields()
{
    CalendarEventSeriesEditRequest request = validSeriesEditRequest();
    request.repeatSeriesId = "  series-1 \t";
    request.startDate = "2026-09-20";
    request.editedStartDate = "2026-09-22";
    request.editedEndDate = "2026-09-25";
    request.title = "Updated workshop";
    request.eventType = "Workshop";
    request.timeStatus = "Timed";
    request.startTime = "08:30";
    request.endTime = "10:15";
    request.allDay = false;

    const std::vector<CalendarEventSeriesEditOccurrenceSnapshot> occurrences{
        {42, "2026-09-21"},
        {7, "2026-10-01"},
        {9, "2026-09-25"}
    };
    const CalendarEventSeriesEditPlanResult plan =
        planCalendarEventSeriesEditSuffix(request, occurrences);

    QVERIFY(plan);
    QCOMPARE(plan.value().size(), std::size_t(3));
    QCOMPARE(plan.value().at(0).eventId, 42);
    QCOMPARE(plan.value().at(0).startDate, std::string("2026-09-23"));
    QCOMPARE(plan.value().at(0).endDate, std::string("2026-09-26"));
    QCOMPARE(plan.value().at(1).eventId, 7);
    QCOMPARE(plan.value().at(1).startDate, std::string("2026-10-03"));
    QCOMPARE(plan.value().at(1).endDate, std::string("2026-10-06"));
    QCOMPARE(plan.value().at(2).eventId, 9);
    QCOMPARE(plan.value().at(2).startDate, std::string("2026-09-27"));
    QCOMPARE(plan.value().at(2).endDate, std::string("2026-09-30"));

    for (const CalendarEventSeriesEditOccurrenceUpdate& update : plan.value())
    {
        QCOMPARE(update.repeatSeriesId, std::string("series-1"));
        QCOMPARE(update.title, request.title);
        QCOMPARE(update.eventType, request.eventType);
        QCOMPARE(update.timeStatus, request.timeStatus);
        QCOMPARE(update.startTime, request.startTime);
        QCOMPARE(update.endTime, request.endTime);
        QCOMPARE(update.allDay, request.allDay);
    }
}

void NextApplicationCalendarEventTests::
seriesEditPlanHandlesEmptyAndAllDaySuffixes()
{
    CalendarEventSeriesEditRequest request = validSeriesEditRequest();
    request.repeatSeriesId = "series-empty";

    const auto emptyPlan = planCalendarEventSeriesEditSuffix(request, {});
    QVERIFY(emptyPlan);
    QVERIFY(emptyPlan.value().empty());

    request.allDay = true;
    request.startTime.reset();
    request.endTime.reset();
    const auto allDayPlan = planCalendarEventSeriesEditSuffix(
        request,
        {{18, "2026-09-21"}}
        );

    QVERIFY(allDayPlan);
    QCOMPARE(allDayPlan.value().size(), std::size_t(1));
    QVERIFY(allDayPlan.value().front().allDay);
    QVERIFY(!allDayPlan.value().front().startTime.has_value());
    QVERIFY(!allDayPlan.value().front().endTime.has_value());
}

void NextApplicationCalendarEventTests::
seriesEditPlanRejectsInvalidSourceAndOutOfRangeDates()
{
    const CalendarEventSeriesEditRequest request = validSeriesEditRequest();
    const auto invalidSource = planCalendarEventSeriesEditSuffix(
        request,
        {{42, "2026-02-30"}}
        );
    QVERIFY(!invalidSource);
    QCOMPARE(invalidSource.error().code, ErrorCode::Technical);
    QVERIFY(!invalidSource.error().message.empty());
    QVERIFY(!invalidSource.error().recoverable);

    CalendarEventSeriesEditRequest upperBoundaryRequest = request;
    upperBoundaryRequest.startDate = "9999-12-30";
    upperBoundaryRequest.editedStartDate = "9999-12-30";
    upperBoundaryRequest.editedEndDate = "9999-12-31";
    upperBoundaryRequest.allDay = true;
    upperBoundaryRequest.startTime.reset();
    upperBoundaryRequest.endTime.reset();
    const auto upperOverflow = planCalendarEventSeriesEditSuffix(
        upperBoundaryRequest,
        {{42, "9999-12-31"}}
        );
    QVERIFY(!upperOverflow);
    QCOMPARE(upperOverflow.error().code, ErrorCode::Technical);

    CalendarEventSeriesEditRequest lowerBoundaryRequest = request;
    lowerBoundaryRequest.startDate = "0001-01-02";
    lowerBoundaryRequest.editedStartDate = "0001-01-01";
    lowerBoundaryRequest.editedEndDate = "0001-01-01";
    lowerBoundaryRequest.allDay = true;
    lowerBoundaryRequest.startTime.reset();
    lowerBoundaryRequest.endTime.reset();
    const auto lowerOverflow = planCalendarEventSeriesEditSuffix(
        lowerBoundaryRequest,
        {{17, "0001-01-01"}}
        );
    QVERIFY(!lowerOverflow);
    QCOMPARE(lowerOverflow.error().code, ErrorCode::Technical);
}

void NextApplicationCalendarEventTests::recordsAndProjectionAreCopyableEqualAndIndependentlyReleasable()
{
    static_assert(std::is_copy_constructible_v<CalendarEventSummary>);
    static_assert(std::is_copy_assignable_v<CalendarEventSummary>);
    static_assert(
        std::is_copy_constructible_v<CalendarEventProjectionInput>
        );
    static_assert(std::is_copy_assignable_v<CalendarEventProjectionInput>);
    static_assert(std::is_copy_constructible_v<CalendarEventProjection>);
    static_assert(std::is_copy_assignable_v<CalendarEventProjection>);

    const auto input = validInput();
    const auto inputCopy = input;
    QVERIFY(inputCopy == input);

    const auto result = CalendarEventProjection::create(input);
    QVERIFY(result);
    CalendarEventProjection original = result.value();
    const CalendarEventProjection copy = original;
    QVERIFY(copy == original);

    CalendarEventProjection assigned;
    assigned = original;
    QVERIFY(assigned == original);

    const auto moved = std::move(assigned);
    QVERIFY(moved == original);
    QVERIFY(original == copy);
}

void NextApplicationCalendarEventTests::contractHasNoMutablePointerOrRichRecordSurface()
{
    static_assert(!HasRawSourceAccessor<CalendarEventSummary>);
    static_assert(!HasRawSourceAccessor<CalendarEventProjectionInput>);
    static_assert(!HasRawSourceAccessor<CalendarEventProjection>);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventProjection>().events()),
        const std::vector<CalendarEventSummary>&
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const CalendarEventProjection>().findEvent(
            std::declval<const CalendarEventId&>()
            )),
        std::optional<CalendarEventSummary>
        >);
    static_assert(!std::is_pointer_v<
        decltype(std::declval<const CalendarEventProjection>().findEvent(
            std::declval<const CalendarEventId&>()
            ))
        >);

    QVERIFY(true);
}

void NextApplicationCalendarEventTests::
campusVisibilityPolicyMatchesNormalizedLiteralCampusTokens()
{
    using Policy = CalendarEventCampusVisibilityPolicy;

    QVERIFY(Policy::eventMatchesCampus(
        "snu meeting",
        campusCodes({"BDG"}),
        campusCodes({"SNU"}),
        true
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "   ",
        campusCodes({"BDG"}),
        campusCodes({"SNU"}),
        false
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "snu meeting",
        campusCodes({"BDG"}),
        {},
        false
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "snu meeting",
        {},
        campusCodes({"SNU"}),
        false
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "general meeting",
        campusCodes({"BDG"}),
        campusCodes({"SNU"}),
        false
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "meeting (bdg)",
        campusCodes({"BDG"}),
        campusCodes({"BDG", "SNU"}),
        false
        ));
    QVERIFY(!Policy::eventMatchesCampus(
        "meeting (snu)",
        campusCodes({"BDG"}),
        campusCodes({"BDG", "SNU"}),
        false
        ));

    QVERIFY(Policy::eventMatchesCampus(
        "meeting (snu) (bdg)",
        campusCodes({"BDG"}),
        campusCodes({"BDG", "SNU"}),
        false
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "open house (s2+)",
        campusCodes({"S2+"}),
        campusCodes({"S2+", "S.2"}),
        false
        ));
    QVERIFY(!Policy::eventMatchesCampus(
        "open house (s.2)",
        campusCodes({"S2+"}),
        campusCodes({"S2+", "S.2"}),
        false
        ));

    QVERIFY(Policy::eventMatchesCampus(
        "campus s20",
        campusCodes({"S2"}),
        campusCodes({"S2"}),
        false
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "xs2y",
        campusCodes({"S2"}),
        campusCodes({"S2"}),
        false
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "xs2y",
        campusCodes({"BDG"}),
        campusCodes({"S2"}),
        false
        ));
    QVERIFY(!Policy::eventMatchesCampus(
        "campus s20",
        campusCodes({"S2"}),
        campusCodes({"S2", "S20"}),
        false
        ));
    QVERIFY(Policy::eventMatchesCampus(
        "campus s2",
        campusCodes({"S2"}),
        campusCodes({"S2", "S20"}),
        false
        ));

    const std::vector<CalendarEventCampusCode> kelvinCurrentCodes{
        {"\xE2\x84\xAA", "k"}
    };
    const std::vector<CalendarEventCampusCode> asciiKnownCodes{
        {"K", "k"}
    };
    QVERIFY(!Policy::eventMatchesCampus(
        "k",
        kelvinCurrentCodes,
        asciiKnownCodes,
        false
        ));
}

void NextApplicationCalendarEventTests::
startOfTermPolicyMatchesCalendarClassificationAndHideSwitch()
{
    using Policy = CalendarEventStartOfTermPolicy;

    for (const std::string_view alias : {
        "new semester",
        "start of term",
        "term start",
        "term starts"
    })
    {
        QVERIFY(Policy::isStartOfTermEvent(alias, "Other"));
        QVERIFY(Policy::shouldHideEvent(alias, "Other", true));
        QVERIFY(!Policy::shouldHideEvent(alias, "Other", false));
    }

    QVERIFY(Policy::isStartOfTermEvent(
        " \tNEW\n  SEMESTER\r ",
        " Other\t "
        ));
    QVERIFY(Policy::isStartOfTermEvent(
        "\xC2\xA0" "StArT\xE2\x80\x83oF\xE2\x80\x83TeRm" "\xC2\xA0",
        "Other"
        ));
    QVERIFY(Policy::isStartOfTermEvent(
        "new" "\xC2\x85" "semester",
        "Other"
        ));

    for (const std::string_view knownType : {
        "Vacation",
        "Holiday",
        "Workshop",
        "CM",
        "Meeting"
    })
    {
        QVERIFY(!Policy::isStartOfTermEvent("term starts", knownType));
    }
    QVERIFY(Policy::isStartOfTermEvent("term starts", " Other "));
    QVERIFY(Policy::isStartOfTermEvent("term starts", "Unrecognized type"));
    QVERIFY(Policy::isStartOfTermEvent("term starts", "other"));
    QVERIFY(!Policy::shouldHideEvent(
        "new semester",
        "Vacation" "\xC2\x85",
        true
        ));

    for (const std::string_view nonmatch : {
        "",
        "new semester celebration",
        "new semestsr",
        "term starting",
        "term-start"
    })
    {
        QVERIFY(!Policy::isStartOfTermEvent(nonmatch, "Other"));
        QVERIFY(!Policy::shouldHideEvent(nonmatch, "Other", true));
    }
    QVERIFY(!Policy::shouldHideEvent("staff meeting", "Meeting", true));
}

QTEST_APPLESS_MAIN(NextApplicationCalendarEventTests)

#include "next_application_calendar_event_tests.moc"

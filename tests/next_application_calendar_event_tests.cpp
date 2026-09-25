#include "next/application/calendar_event_edit_draft.h"
#include "next/application/calendar_event_delete_all_port.h"
#include "next/application/calendar_event_import_save_port.h"
#include "next/application/calendar_event_projection.h"
#include "next/application/calendar_event_save_port.h"
#include "next/application/calendar_event_series_create_port.h"
#include "next/application/calendar_event_series_edit_port.h"

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

template <typename Value>
concept HasRawSourceAccessor = requires(const Value& value)
{
    value.rawSource();
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
    void timingErrorsKeepFeatureMessagesAndValidationPrecedence();
    void calendarEventNameValidationUsesDomainClassifiersAndPreservesRawText();
    void timingAcceptsFullGregorianRangeAndCrossDayClocks();
    void saveRequestContractHasNoQtOrLegacySurface();
    void importSaveRequestBoundsOrderedCreateBatchAndNoOp();
    void importSaveRequestRejectsUpdatesAndInvalidEvents();
    void importSaveRequestContractHasNoQtOrLegacySurface();
    void deleteAllPortContractUsesStructuredQtFreeResult();
    void editDraftBoundsAndTimeStatusPolicyIsExplicit();
    void editDraftIsCopyableEqualAndIndependentlyReleasable();
    void editDraftContractHasNoQtOrLegacySurface();
    void seriesCreateRequestBoundsAndOccurrencesRemainTyped();
    void seriesCreateRequestContractHasNoQtOrLegacySurface();
    void seriesEditRequestBoundsAndDatesRemainTyped();
    void seriesEditRequestAllDayAndTimeStatusPolicyIsExplicit();
    void seriesEditRequestContractHasNoQtOrLegacySurface();
    void recordsAndProjectionAreCopyableEqualAndIndependentlyReleasable();
    void contractHasNoMutablePointerOrRichRecordSurface();
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

QTEST_APPLESS_MAIN(NextApplicationCalendarEventTests)

#include "next_application_calendar_event_tests.moc"

#include "next/domain/birthday_month_day.h"
#include "next/domain/calendar_event_timing.h"
#include "next/domain/course.h"
#include "next/domain/course_catalog_identity.h"
#include "next/domain/document_content_reference.h"
#include "next/domain/domain_types.h"
#include "next/domain/korean_teacher_key.h"
#include "next/domain/operation_result.h"
#include "next/domain/schedule_entry.h"
#include "next/domain/schedule_time.h"
#include "next/domain/speaking_evaluation_grade.h"
#include "next/domain/student_name_pair.h"
#include "next/domain/teacher_display_name.h"

#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Domain;

class NextDomainContractTests final : public QObject
{
    Q_OBJECT

private slots:
    void typedIdentifiersRejectEmptyValues();
    void typedIdentifiersRemainDistinct();
    void documentContentReferencesHaveValueSemantics();
    void birthdayMonthDaysValidateAgainstLeapYear2000();
    void resultCarriesValueOrStructuredError();
    void scheduleTimesAcceptWeekdayAndMinuteBoundaries();
    void scheduleTimesRejectInvalidDaysAndIntervals();
    void scheduleTimesHaveValueAndOverlapSemantics();
    void calendarEventNamesClassifyKnownValuesAndRejectUnknownTokens();
    void calendarEventTimingsValidateGregorianDateBoundaries();
    void calendarEventTimingsEnforceDateAndClockOrdering();
    void calendarEventTimingsEnforceTimeAndStatusPolicy();
    void scheduleEntriesKeepTypedClassAndValidatedTime();
    void coursesExposeOrderedSupportedPairs();
    void coursesHaveValueAndAccessorSemantics();
    void coursesRejectInvalidNamesAndCrossGradePairs();
    void courseGradeBandsClassifyGradeWithoutLevelValidation();
    void speakingEvaluationGradesParseExactSupportedLabels();
    void speakingEvaluationGradeLabelsRejectUnknownValues();
    void speakingEvaluationGradesAggregateAllSixComponents();
    void speakingEvaluationGradeAggregationRejectsInvalidOrMissingScores();
    void speakingEvaluationGradeAggregationClampsToSupportedRange();
    void coursesExposeWeeklyMeetingDayRules();
    void weeklyMeetingDayRulesRejectUnknownCourseClassificationValues();
    void weeklyMeetingDayRulesRejectInvalidPatterns();
    void koreanTeacherKeysKeepEveryAcceptedRangeAndBoundary();
    void koreanTeacherKeysDiscardOtherCodeUnitsWithoutNormalization();
    void koreanTeacherKeysExposeEmptyAndValueSemantics();
    void studentNamePairsRequireBothNames();
    void studentNamePairsCompareExactUtf16PartsAndOrder();
    void studentNamePairsPreserveValuesAcrossRvalueOperations();
    void duplicateStudentNamePairGroupsAreExactAndDeterministic();
    void studentNamePairsKeepPartsDistinctAcrossDelimiter();
    void teacherDisplayNamesSelectPreferredAndEnglishNames();
    void teacherDisplayNamesSelectRomanizationAndKoreanNames();
    void teacherDisplayNamesKeepEmptyAndCopiedValues();
};

void NextDomainContractTests::typedIdentifiersRejectEmptyValues()
{
    QVERIFY(!WorkspaceId::fromString("").has_value());

    const auto workspaceId = WorkspaceId::fromString("workspace-1");
    QVERIFY(workspaceId.has_value());
    QVERIFY(workspaceId->value() == "workspace-1");
}

void NextDomainContractTests::typedIdentifiersRemainDistinct()
{
    static_assert(!std::is_same_v<WorkspaceId, TeacherId>);
    static_assert(!std::is_same_v<ClassId, CampusId>);

    const auto first = ClassId::fromString("class-1");
    const auto second = ClassId::fromString("class-1");
    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    QVERIFY(*first == *second);

    const auto moveConstructionValue = ClassId::fromString("class-move");
    QVERIFY(moveConstructionValue.has_value());
    ClassId moveConstructionSource = *moveConstructionValue;
    const std::string moveConstructionSnapshot =
        moveConstructionSource.value();
    const ClassId moveConstructed(std::move(moveConstructionSource));
    QVERIFY(moveConstructionSource.value() == moveConstructionSnapshot);
    QVERIFY(moveConstructed.value() == moveConstructionSnapshot);

    const auto moveAssignmentValue = ClassId::fromString("class-assign");
    const auto initialDestinationValue = ClassId::fromString("class-before");
    QVERIFY(moveAssignmentValue.has_value());
    QVERIFY(initialDestinationValue.has_value());
    ClassId moveAssignmentSource = *moveAssignmentValue;
    const std::string moveAssignmentSnapshot = moveAssignmentSource.value();
    ClassId moveAssignmentDestination = *initialDestinationValue;
    moveAssignmentDestination = std::move(moveAssignmentSource);
    QVERIFY(moveAssignmentSource.value() == moveAssignmentSnapshot);
    QVERIFY(moveAssignmentDestination.value() == moveAssignmentSnapshot);
}

void NextDomainContractTests::documentContentReferencesHaveValueSemantics()
{
    static_assert(std::is_same_v<DocumentContentReference::Text, std::string>);
    static_assert(std::is_copy_constructible_v<DocumentContentReference>);
    static_assert(std::is_copy_assignable_v<DocumentContentReference>);

    const DocumentContentReference empty;
    QVERIFY(empty.empty());
    QVERIFY(empty.value().empty());
    QVERIFY(empty.text().empty());

    const DocumentContentReference explicitValue(
        DocumentContentReference::Text("resource://documents/lesson.pdf")
        );
    QVERIFY(!explicitValue.empty());
    QVERIFY(explicitValue.value() == "resource://documents/lesson.pdf");
    QVERIFY(explicitValue.text() == explicitValue.value());

    const DocumentContentReference copied(explicitValue);
    QVERIFY(copied == explicitValue);
    QVERIFY(copied.value() == explicitValue.text());

    DocumentContentReference assigned;
    assigned = explicitValue;
    QVERIFY(assigned == explicitValue);
}

void NextDomainContractTests::birthdayMonthDaysValidateAgainstLeapYear2000()
{
    static_assert(!std::is_default_constructible_v<BirthdayMonthDay>);
    static_assert(std::is_copy_constructible_v<BirthdayMonthDay>);
    static_assert(!std::is_copy_assignable_v<BirthdayMonthDay>);

    const auto leapDay = BirthdayMonthDay::fromComponents(2, 29);
    QVERIFY(leapDay.has_value());
    QCOMPARE(leapDay->month(), 2);
    QCOMPARE(leapDay->day(), 29);

    const auto equalLeapDay = BirthdayMonthDay::fromComponents(2, 29);
    const auto firstDayOfYear = BirthdayMonthDay::fromComponents(1, 1);
    QVERIFY(equalLeapDay.has_value());
    QVERIFY(firstDayOfYear.has_value());
    QVERIFY(*leapDay == *equalLeapDay);
    QVERIFY(*leapDay != *firstDayOfYear);

    QVERIFY(BirthdayMonthDay::fromComponents(12, 31).has_value());
    QVERIFY(!BirthdayMonthDay::fromComponents(0, 1).has_value());
    QVERIFY(!BirthdayMonthDay::fromComponents(13, 1).has_value());
    QVERIFY(!BirthdayMonthDay::fromComponents(2, 0).has_value());
    QVERIFY(!BirthdayMonthDay::fromComponents(2, 30).has_value());
    QVERIFY(!BirthdayMonthDay::fromComponents(4, 31).has_value());
}

void NextDomainContractTests::resultCarriesValueOrStructuredError()
{
    const auto success = Result<int>::success(42);
    QVERIFY(success);
    QCOMPARE(success.value(), 42);

    const auto failure = Result<int>::failure(
        OperationError{
            .code = ErrorCode::Conflict,
            .message = "The import has conflicts.",
            .recoverable = true
        }
        );
    QVERIFY(!failure);
    QCOMPARE(failure.error().code, ErrorCode::Conflict);
    QVERIFY(failure.error().message == "The import has conflicts.");
    QVERIFY(failure.error().recoverable);

    const auto completed = Result<void>::success();
    QVERIFY(completed);
}

void NextDomainContractTests::scheduleTimesAcceptWeekdayAndMinuteBoundaries()
{
    for (int weekdayIndex = 0; weekdayIndex <= 6; ++weekdayIndex)
    {
        const auto value = ScheduleTime::fromMinutes(weekdayIndex, 0, 1);
        QVERIFY(value.has_value());
        QCOMPARE(value->weekdayIndex(), weekdayIndex);
    }

    const auto mondayAtStartOfDay = ScheduleTime::fromMinutes(
        static_cast<int>(Weekday::Monday),
        0,
        1
        );
    const auto sundayAtEndOfDay = ScheduleTime::fromMinutes(
        static_cast<int>(Weekday::Sunday),
        1438,
        1439
        );
    QVERIFY(mondayAtStartOfDay.has_value());
    QVERIFY(sundayAtEndOfDay.has_value());
    QVERIFY(mondayAtStartOfDay->weekday() == Weekday::Monday);
    QVERIFY(sundayAtEndOfDay->weekday() == Weekday::Sunday);
    QCOMPARE(sundayAtEndOfDay->startMinute(), 1438);
    QCOMPARE(sundayAtEndOfDay->endMinute(), 1439);
}

void NextDomainContractTests::scheduleTimesRejectInvalidDaysAndIntervals()
{
    QVERIFY(!ScheduleTime::fromMinutes(-1, 0, 1).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(7, 0, 1).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, -1, 1).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, 1440, 1441).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, 0, -1).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, 12, 12).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(0, 13, 12).has_value());
    QVERIFY(!ScheduleTime::fromMinutes(6, 1439, 1440).has_value());
}

void NextDomainContractTests::scheduleTimesHaveValueAndOverlapSemantics()
{
    const auto first = ScheduleTime::fromMinutes(0, 0, 60);
    const auto sameValue = ScheduleTime::fromMinutes(0, 0, 60);
    const auto adjacent = ScheduleTime::fromMinutes(0, 60, 120);
    const auto overlapping = ScheduleTime::fromMinutes(0, 59, 120);
    const auto differentWeekday = ScheduleTime::fromMinutes(1, 0, 60);
    QVERIFY(first.has_value());
    QVERIFY(sameValue.has_value());
    QVERIFY(adjacent.has_value());
    QVERIFY(overlapping.has_value());
    QVERIFY(differentWeekday.has_value());

    const ScheduleTime copy = *first;
    QVERIFY(copy == *first);
    QVERIFY(copy == *sameValue);
    QVERIFY(copy != *adjacent);
    QVERIFY(!first->overlaps(*adjacent));
    QVERIFY(first->overlaps(*overlapping));
    QVERIFY(!first->overlaps(*differentWeekday));
}

void NextDomainContractTests::
calendarEventNamesClassifyKnownValuesAndRejectUnknownTokens()
{
    const std::array eventTypes = {
        std::pair{std::string_view("Vacation"), CalendarEventType::Vacation},
        std::pair{std::string_view("Holiday"), CalendarEventType::Holiday},
        std::pair{std::string_view("Workshop"), CalendarEventType::Workshop},
        std::pair{std::string_view("CM"), CalendarEventType::CM},
        std::pair{std::string_view("Meeting"), CalendarEventType::Meeting},
        std::pair{std::string_view("Other"), CalendarEventType::Other}
    };
    for (const auto& [name, expected] : eventTypes)
    {
        const auto actual = calendarEventTypeFromName(name);
        QVERIFY(actual.has_value());
        QVERIFY(*actual == expected);
    }

    for (const std::string_view unknown : {
             "",
             "vacation",
             "Workshop ",
             "CM2",
             "Conference"
         })
    {
        QVERIFY(!calendarEventTypeFromName(unknown).has_value());
    }

    const std::array timeStatuses = {
        std::pair{std::string_view("Timed"), CalendarEventTimeStatus::Timed},
        std::pair{
            std::string_view("Unknown"),
            CalendarEventTimeStatus::Unknown
        },
        std::pair{
            std::string_view("Unconfirmed"),
            CalendarEventTimeStatus::Unconfirmed
        }
    };
    for (const auto& [name, expected] : timeStatuses)
    {
        const auto actual = calendarEventTimeStatusFromName(name);
        QVERIFY(actual.has_value());
        QVERIFY(*actual == expected);
    }

    for (const std::string_view unknown : {
             "",
             "timed",
             "Timed ",
             "Pending",
             "Not applicable"
         })
    {
        QVERIFY(!calendarEventTimeStatusFromName(unknown).has_value());
    }
}

void NextDomainContractTests::
calendarEventTimingsValidateGregorianDateBoundaries()
{
    QVERIFY(CalendarEventTiming::isCanonicalDate("0001-01-01"));
    QVERIFY(CalendarEventTiming::isCanonicalDate("9999-12-31"));
    QVERIFY(CalendarEventTiming::isCanonicalDate("2000-02-29"));
    QVERIFY(CalendarEventTiming::isCanonicalDate("2024-02-29"));
    QVERIFY(!CalendarEventTiming::isCanonicalDate("1900-02-29"));
    QVERIFY(!CalendarEventTiming::isCanonicalDate("2023-02-29"));
    QVERIFY(!CalendarEventTiming::isCanonicalDate("0000-01-01"));
    QVERIFY(!CalendarEventTiming::isCanonicalDate("2026-2-01"));
    QVERIFY(!CalendarEventTiming::isCanonicalDate("2026/02/01"));
    QVERIFY(!CalendarEventTiming::isCanonicalDate("2026006010"));
    QVERIFY(!CalendarEventTiming::isCanonicalDate("2026-06110"));

    const CalendarEventTiming valid(
        "2024-02-29",
        "2024-02-29",
        std::optional<std::string>("09:00"),
        std::optional<std::string>("10:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(!valid.validate().has_value());

    const CalendarEventTiming invalidDate(
        "1900-02-29",
        "1900-02-29",
        std::optional<std::string>("09:00"),
        std::optional<std::string>("10:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        invalidDate.validate()
        == CalendarEventTimingIssue::InvalidDate
        );
}

void NextDomainContractTests::
calendarEventTimingsEnforceDateAndClockOrdering()
{
    const CalendarEventTiming reversedDates(
        "2026-09-21",
        "2026-09-20",
        std::optional<std::string>("09:00"),
        std::optional<std::string>("10:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        reversedDates.validate()
        == CalendarEventTimingIssue::EndDateBeforeStartDate
        );

    const CalendarEventTiming sameTime(
        "2026-09-20",
        "2026-09-20",
        std::optional<std::string>("09:00"),
        std::optional<std::string>("09:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        sameTime.validate()
        == CalendarEventTimingIssue::EndTimeMustFollowStartTime
        );

    const CalendarEventTiming reversedTimes(
        "2026-09-20",
        "2026-09-20",
        std::optional<std::string>("10:00"),
        std::optional<std::string>("09:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        reversedTimes.validate()
        == CalendarEventTimingIssue::EndTimeMustFollowStartTime
        );

    const CalendarEventTiming overnight(
        "2026-09-20",
        "2026-09-21",
        std::optional<std::string>("23:59"),
        std::optional<std::string>("00:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(!overnight.validate().has_value());

    const CalendarEventTiming crossDayEqualTimes(
        "2026-09-20",
        "2026-09-21",
        std::optional<std::string>("12:00"),
        std::optional<std::string>("12:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(!crossDayEqualTimes.validate().has_value());
}

void NextDomainContractTests::
calendarEventTimingsEnforceTimeAndStatusPolicy()
{
    const CalendarEventTiming partialTimes(
        "2026-09-20",
        "2026-09-20",
        std::optional<std::string>("09:00"),
        std::nullopt,
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        partialTimes.validate()
        == CalendarEventTimingIssue::TimesMustBePaired
        );

    const CalendarEventTiming malformedTime(
        "2026-09-20",
        "2026-09-20",
        std::optional<std::string>("9:00"),
        std::optional<std::string>("10:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        malformedTime.validate()
        == CalendarEventTimingIssue::InvalidTime
        );

    const CalendarEventTiming outOfRangeTime(
        "2026-09-20",
        "2026-09-20",
        std::optional<std::string>("23:59"),
        std::optional<std::string>("24:00"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        outOfRangeTime.validate()
        == CalendarEventTimingIssue::InvalidTime
        );

    const CalendarEventTiming minuteBoundaries(
        "2026-09-20",
        "2026-09-20",
        std::optional<std::string>("00:00"),
        std::optional<std::string>("23:59"),
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(!minuteBoundaries.validate().has_value());

    const CalendarEventTiming allDay(
        "2026-09-20",
        "2026-09-20",
        std::nullopt,
        std::nullopt,
        true,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(!allDay.validate().has_value());

    const CalendarEventTiming allDayWithUnconfirmedStatus(
        "2026-09-20",
        "2026-09-20",
        std::nullopt,
        std::nullopt,
        true,
        CalendarEventTimeStatus::Unconfirmed
        );
    QVERIFY(
        allDayWithUnconfirmedStatus.validate()
        == CalendarEventTimingIssue::AllDayRequiresTimedStatusAndNoTimes
        );

    const CalendarEventTiming allDayWithTimes(
        "2026-09-20",
        "2026-09-20",
        std::optional<std::string>("09:00"),
        std::optional<std::string>("10:00"),
        true,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        allDayWithTimes.validate()
        == CalendarEventTimingIssue::AllDayRequiresTimedStatusAndNoTimes
        );

    const CalendarEventTiming timedWithoutTimes(
        "2026-09-20",
        "2026-09-20",
        std::nullopt,
        std::nullopt,
        false,
        CalendarEventTimeStatus::Timed
        );
    QVERIFY(
        timedWithoutTimes.validate()
        == CalendarEventTimingIssue::TimedRequiresBothTimes
        );

    const CalendarEventTiming invalidTimeStatus(
        "2026-09-20",
        "2026-09-20",
        std::nullopt,
        std::nullopt,
        false,
        static_cast<CalendarEventTimeStatus>(99)
        );
    QVERIFY(
        invalidTimeStatus.validate()
        == CalendarEventTimingIssue::InvalidTimeStatus
        );

    const CalendarEventTiming invalidAllDayTimeStatus(
        "2026-09-20",
        "2026-09-20",
        std::nullopt,
        std::nullopt,
        true,
        static_cast<CalendarEventTimeStatus>(99)
        );
    QVERIFY(
        invalidAllDayTimeStatus.validate()
        == CalendarEventTimingIssue::InvalidTimeStatus
        );

    for (const CalendarEventTimeStatus status : {
             CalendarEventTimeStatus::Unknown,
             CalendarEventTimeStatus::Unconfirmed
         })
    {
        const CalendarEventTiming noTimes(
            "2026-09-20",
            "2026-09-20",
            std::nullopt,
            std::nullopt,
            false,
            status
            );
        QVERIFY(!noTimes.validate().has_value());

        const CalendarEventTiming withTimes(
            "2026-09-20",
            "2026-09-20",
            std::optional<std::string>("09:00"),
            std::optional<std::string>("10:00"),
            false,
            status
            );
        QVERIFY(
            withTimes.validate()
            == CalendarEventTimingIssue::NonTimedStatusRequiresNoTimes
            );
    }
}

void NextDomainContractTests::scheduleEntriesKeepTypedClassAndValidatedTime()
{
    static_assert(
        std::is_constructible_v<ScheduleEntry, ClassId, ScheduleTime>
        );
    static_assert(
        !std::is_constructible_v<ScheduleEntry, TeacherId, ScheduleTime>
        );

    const auto classId = ClassId::fromString("class-42");
    const auto scheduleTime = ScheduleTime::fromMinutes(
        static_cast<int>(Weekday::Tuesday),
        16 * 60,
        16 * 60 + 55
        );
    QVERIFY(classId.has_value());
    QVERIFY(scheduleTime.has_value());

    const ScheduleEntry entry(*classId, *scheduleTime);
    const ScheduleEntry copy = entry;
    const ScheduleEntry differentClass(
        *ClassId::fromString("class-43"),
        *scheduleTime
        );
    const ScheduleEntry differentTime(
        *classId,
        *ScheduleTime::fromMinutes(
            static_cast<int>(Weekday::Tuesday),
            16 * 60,
            17 * 60
            )
        );

    QVERIFY(entry.classId() == *classId);
    QVERIFY(entry.scheduleTime() == *scheduleTime);
    QVERIFY(copy == entry);
    QVERIFY(copy != differentClass);
    QVERIFY(copy != differentTime);
    QVERIFY(entry.scheduleTime().weekday() == Weekday::Tuesday);
    QCOMPARE(entry.scheduleTime().startMinute(), 16 * 60);
    QCOMPARE(entry.scheduleTime().endMinute(), 16 * 60 + 55);
}

void NextDomainContractTests::coursesExposeOrderedSupportedPairs()
{
    using CourseNames = std::pair<std::string_view, std::string_view>;
    const std::array<CourseNames, 25> supportedPairs{{
        {"E4", "Theseus"},
        {"E4", "Perseus"},
        {"E4", "Odysseus"},
        {"E4", "Hercules"},
        {"E5", "Artemis"},
        {"E5", "Hermes"},
        {"E5", "Apollo"},
        {"E5", "Zeus"},
        {"E5", "Athena"},
        {"E6", "Helios"},
        {"E6", "Poseidon"},
        {"E6", "Gaia"},
        {"E6", "Hera"},
        {"E6", "Song's"},
        {"M1", "Elephantus"},
        {"M1", "Galaxia"},
        {"M1", "Solis"},
        {"M1", "Major"},
        {"M1", "Song's"},
        {"M2", "Ursa"},
        {"M2", "Leo"},
        {"M2", "Tigris"},
        {"M2", "Major"},
        {"M2", "Song's"},
        {"M3", "Song's"}
    }};
    const std::array<CourseCatalogIdentity, 25> expectedIdentities{{
        CourseCatalogIdentity::E4Theseus,
        CourseCatalogIdentity::E4Perseus,
        CourseCatalogIdentity::E4Odysseus,
        CourseCatalogIdentity::E4Hercules,
        CourseCatalogIdentity::E5Artemis,
        CourseCatalogIdentity::E5Hermes,
        CourseCatalogIdentity::E5Apollo,
        CourseCatalogIdentity::E5Zeus,
        CourseCatalogIdentity::E5Athena,
        CourseCatalogIdentity::E6Helios,
        CourseCatalogIdentity::E6Poseidon,
        CourseCatalogIdentity::E6Gaia,
        CourseCatalogIdentity::E6Hera,
        CourseCatalogIdentity::E6Songs,
        CourseCatalogIdentity::M1Elephantus,
        CourseCatalogIdentity::M1Galaxia,
        CourseCatalogIdentity::M1Solis,
        CourseCatalogIdentity::M1Major,
        CourseCatalogIdentity::M1Songs,
        CourseCatalogIdentity::M2Ursa,
        CourseCatalogIdentity::M2Leo,
        CourseCatalogIdentity::M2Tigris,
        CourseCatalogIdentity::M2Major,
        CourseCatalogIdentity::M2Songs,
        CourseCatalogIdentity::M3Songs
    }};
    const std::array<std::string_view, 6> expectedGrades{
        "E4", "E5", "E6", "M1", "M2", "M3"
    };
    const std::array<std::vector<std::string_view>, 6> expectedLevels{{
        {"Theseus", "Perseus", "Odysseus", "Hercules"},
        {"Artemis", "Hermes", "Apollo", "Zeus", "Athena"},
        {"Helios", "Poseidon", "Gaia", "Hera", "Song's"},
        {"Elephantus", "Galaxia", "Solis", "Major", "Song's"},
        {"Ursa", "Leo", "Tigris", "Major", "Song's"},
        {"Song's"}
    }};

    const auto grades = Course::grades();
    QCOMPARE(static_cast<int>(grades.size()),
        static_cast<int>(expectedGrades.size()));
    for (std::size_t index = 0; index < expectedGrades.size(); ++index)
    {
        QVERIFY(grades[index] == expectedGrades[index]);
        QVERIFY(
            Course::levelsForGrade(expectedGrades[index])
            == expectedLevels[index]
            );
    }

    static_assert(std::is_same_v<
        decltype(std::declval<const Course&>().catalogIdentity()),
        CourseCatalogIdentity
        >);

    std::vector<CourseCatalogIdentity> observedIdentities;
    observedIdentities.reserve(supportedPairs.size());
    for (std::size_t index = 0; index < supportedPairs.size(); ++index)
    {
        const CourseNames& names = supportedPairs[index];
        const auto course = Course::fromNames(names.first, names.second);
        QVERIFY(course.has_value());
        QVERIFY(course->grade() == names.first);
        QVERIFY(course->level() == names.second);
        QVERIFY(course->catalogIdentity() == expectedIdentities[index]);
        QVERIFY(std::find(
            observedIdentities.begin(),
            observedIdentities.end(),
            course->catalogIdentity()
            ) == observedIdentities.end());
        observedIdentities.push_back(course->catalogIdentity());
    }
    QCOMPARE(static_cast<int>(observedIdentities.size()), 25);
}

void NextDomainContractTests::coursesHaveValueAndAccessorSemantics()
{
    const auto first = Course::fromNames("E4", "Theseus");
    const auto sameValue = Course::fromNames("E4", "Theseus");
    const auto otherLevel = Course::fromNames("E4", "Perseus");
    const auto e6Songs = Course::fromNames("E6", "Song's");
    const auto m1Songs = Course::fromNames("M1", "Song's");
    const auto m2Songs = Course::fromNames("M2", "Song's");
    const auto m3Songs = Course::fromNames("M3", "Song's");
    const auto m1Major = Course::fromNames("M1", "Major");
    const auto m2Major = Course::fromNames("M2", "Major");
    QVERIFY(first.has_value());
    QVERIFY(sameValue.has_value());
    QVERIFY(otherLevel.has_value());
    QVERIFY(e6Songs.has_value());
    QVERIFY(m1Songs.has_value());
    QVERIFY(m2Songs.has_value());
    QVERIFY(m3Songs.has_value());
    QVERIFY(m1Major.has_value());
    QVERIFY(m2Major.has_value());
    QVERIFY(*first == *sameValue);
    QVERIFY(*first != *otherLevel);
    QVERIFY(first->grade() == "E4");
    QVERIFY(first->level() == "Theseus");
    QVERIFY(e6Songs->level() == m1Songs->level());
    QVERIFY(m1Songs->level() == m2Songs->level());
    QVERIFY(m2Songs->level() == m3Songs->level());
    QVERIFY(e6Songs->catalogIdentity() != m1Songs->catalogIdentity());
    QVERIFY(m1Songs->catalogIdentity() != m2Songs->catalogIdentity());
    QVERIFY(m2Songs->catalogIdentity() != m3Songs->catalogIdentity());
    QVERIFY(*e6Songs != *m1Songs);
    QVERIFY(*m1Songs != *m2Songs);
    QVERIFY(*m2Songs != *m3Songs);
    QVERIFY(m1Major->level() == m2Major->level());
    QVERIFY(m1Major->catalogIdentity() != m2Major->catalogIdentity());
    QVERIFY(*m1Major != *m2Major);
}

void NextDomainContractTests::coursesRejectInvalidNamesAndCrossGradePairs()
{
    QVERIFY(!Course::fromNames("", "Theseus").has_value());
    QVERIFY(!Course::fromNames("E7", "Theseus").has_value());
    QVERIFY(!Course::fromNames("E4", "Unknown").has_value());
    QVERIFY(!Course::fromNames("E4", "Zeus").has_value());
    QVERIFY(!Course::fromNames("M3", "Zeus").has_value());
    QVERIFY(!Course::fromNames("e4", "Theseus").has_value());
    QVERIFY(!Course::fromNames("E4", "theseus").has_value());
    QVERIFY(!Course::fromNames(" E4", "Theseus").has_value());
    QVERIFY(!Course::fromNames("E4 ", "Theseus").has_value());
    QVERIFY(!Course::fromNames("E4", " Theseus").has_value());
    QVERIFY(!Course::fromNames("E4", "Theseus ").has_value());
    QVERIFY(!Course::fromNames("E4", "TheSeus").has_value());
    QVERIFY(!Course::fromNames("E4", "Theeseus").has_value());
    QVERIFY(Course::levelsForGrade("E7").empty());
}

void NextDomainContractTests::
    courseGradeBandsClassifyGradeWithoutLevelValidation()
{
    const std::array<std::pair<std::string_view, CourseGradeBand>, 7> cases{{
        {"E4", CourseGradeBand::E4},
        {"E5", CourseGradeBand::E5},
        {"E6", CourseGradeBand::E6},
        {"M1", CourseGradeBand::M1},
        {"M2", CourseGradeBand::M2},
        {"M3", CourseGradeBand::M3},
        {"E7", CourseGradeBand::Other}
    }};

    for (const auto& [grade, expected] : cases)
    {
        QVERIFY(Course::gradeBandForName(grade) == expected);
    }
    QVERIFY(Course::gradeBandForName("") == CourseGradeBand::Other);
    QVERIFY(Course::gradeBandForName("m1") == CourseGradeBand::Other);
    QVERIFY(Course::gradeBandForName(" M1 ") == CourseGradeBand::Other);
    QVERIFY(!Course::fromNames("M1", "Unknown").has_value());
    QVERIFY(
        Course::gradeBandForName("M1") == CourseGradeBand::M1
    );
}

void NextDomainContractTests::
    speakingEvaluationGradesParseExactSupportedLabels()
{
    struct GradeCase
    {
        std::string_view label;
        int value;
        SpeakingEvaluationGrade grade;
    };
    const std::array<GradeCase, 5> grades{{
            {"C", 1, SpeakingEvaluationGrade::C},
            {"B", 2, SpeakingEvaluationGrade::B},
            {"B+", 3, SpeakingEvaluationGrade::BPlus},
            {"A", 4, SpeakingEvaluationGrade::A},
            {"A+", 5, SpeakingEvaluationGrade::APlus}
        }};

    for (const GradeCase& gradeCase : grades)
    {
        const auto parsed = speakingEvaluationGradeFromLabel(gradeCase.label);
        QVERIFY(parsed.has_value());
        QVERIFY(*parsed == gradeCase.grade);
        const auto label = speakingEvaluationGradeLabel(*parsed);
        QVERIFY(label.has_value());
        QVERIFY(*label == gradeCase.label);
        QVERIFY(
            speakingEvaluationGradeFromValue(gradeCase.value)
            == gradeCase.grade
            );
    }

    for (const std::string_view invalid : {
             "", "c", " B", "B ", "A-", "N/A"
         })
    {
        QVERIFY(!speakingEvaluationGradeFromLabel(invalid).has_value());
    }
    QVERIFY(!speakingEvaluationGradeFromValue(0).has_value());
    QVERIFY(!speakingEvaluationGradeFromValue(6).has_value());
}

void NextDomainContractTests::
    speakingEvaluationGradeLabelsRejectUnknownValues()
{
    QVERIFY(
        !speakingEvaluationGradeLabel(
            static_cast<SpeakingEvaluationGrade>(0)
            ).has_value()
        );
    QVERIFY(
        !speakingEvaluationGradeLabel(
            static_cast<SpeakingEvaluationGrade>(6)
            ).has_value()
        );
}

void NextDomainContractTests::
    speakingEvaluationGradesAggregateAllSixComponents()
{
    static_assert(SpeakingEvaluationCriterionCount == 6);

    // Exhaust every one of the 5^6 valid six-component combinations.
    constexpr int CombinationCount = 15625;
    for (int combination = 0; combination < CombinationCount; ++combination)
    {
        int encoded = combination;
        int sum = 0;
        SpeakingEvaluationComponentScores scores{};
        for (std::size_t index = 0; index < scores.size(); ++index)
        {
            const int value = encoded % 5 + 1;
            encoded /= 5;
            sum += value;
            scores[index] = speakingEvaluationGradeFromValue(value);
        }

        const double average = static_cast<double>(sum) / scores.size();
        int expectedValue = static_cast<int>(average);
        if (average - expectedValue >= 0.4)
        {
            ++expectedValue;
        }
        expectedValue = std::clamp(expectedValue, 1, 5);

        const auto actual = calculateOverallSpeakingEvaluationGrade(scores);
        QVERIFY(actual.has_value());
        const auto expected = speakingEvaluationGradeFromValue(expectedValue);
        QVERIFY(expected.has_value());
        QVERIFY(*actual == *expected);
    }

    // Six values cannot produce an exact 0.4 fraction: 14/6 stays at B,
    // while 15/6 crosses the existing >= 0.4 rounding boundary to B+.
    const SpeakingEvaluationComponentScores belowRoundingThreshold{
        SpeakingEvaluationGrade::C,
        SpeakingEvaluationGrade::C,
        SpeakingEvaluationGrade::B,
        SpeakingEvaluationGrade::BPlus,
        SpeakingEvaluationGrade::BPlus,
        SpeakingEvaluationGrade::A
    };
    QVERIFY(
        calculateOverallSpeakingEvaluationGrade(belowRoundingThreshold)
        == SpeakingEvaluationGrade::B
        );

    const SpeakingEvaluationComponentScores atRoundingThreshold{
        SpeakingEvaluationGrade::C,
        SpeakingEvaluationGrade::C,
        SpeakingEvaluationGrade::B,
        SpeakingEvaluationGrade::BPlus,
        SpeakingEvaluationGrade::A,
        SpeakingEvaluationGrade::A
    };
    QVERIFY(
        calculateOverallSpeakingEvaluationGrade(atRoundingThreshold)
        == SpeakingEvaluationGrade::BPlus
        );

    const SpeakingEvaluationComponentScores mixedSixteenOfSix{
        SpeakingEvaluationGrade::APlus,
        SpeakingEvaluationGrade::C,
        SpeakingEvaluationGrade::BPlus,
        SpeakingEvaluationGrade::B,
        SpeakingEvaluationGrade::C,
        SpeakingEvaluationGrade::A
    };
    QVERIFY(
        calculateOverallSpeakingEvaluationGrade(mixedSixteenOfSix)
        == SpeakingEvaluationGrade::BPlus
        );
}

void NextDomainContractTests::
    speakingEvaluationGradeAggregationRejectsInvalidOrMissingScores()
{
    SpeakingEvaluationComponentScores missing{};
    missing.fill(SpeakingEvaluationGrade::BPlus);
    missing[static_cast<std::size_t>(SpeakingEvaluationCriterion::Content)] =
        std::nullopt;
    QVERIFY(!calculateOverallSpeakingEvaluationGrade(missing).has_value());

    SpeakingEvaluationComponentScores invalid{};
    invalid.fill(SpeakingEvaluationGrade::BPlus);
    invalid[static_cast<std::size_t>(SpeakingEvaluationCriterion::Fluency)] =
        static_cast<SpeakingEvaluationGrade>(0);
    QVERIFY(!calculateOverallSpeakingEvaluationGrade(invalid).has_value());
}

void NextDomainContractTests::
    speakingEvaluationGradeAggregationClampsToSupportedRange()
{
    SpeakingEvaluationComponentScores allLowest{};
    allLowest.fill(SpeakingEvaluationGrade::C);
    QVERIFY(
        calculateOverallSpeakingEvaluationGrade(allLowest)
        == SpeakingEvaluationGrade::C
        );

    SpeakingEvaluationComponentScores allHighest{};
    allHighest.fill(SpeakingEvaluationGrade::APlus);
    QVERIFY(
        calculateOverallSpeakingEvaluationGrade(allHighest)
        == SpeakingEvaluationGrade::APlus
        );
}

void NextDomainContractTests::coursesExposeWeeklyMeetingDayRules()
{
    const std::vector<Course::WeeklyMeetingDayPattern> pairedPatterns{
        {Weekday::Monday, Weekday::Wednesday},
        {Weekday::Monday, Weekday::Friday},
        {Weekday::Wednesday, Weekday::Friday},
        {Weekday::Tuesday, Weekday::Thursday}
    };
    const std::vector<Course::WeeklyMeetingDayPattern> threeDayPatterns{
        {Weekday::Monday, Weekday::Wednesday, Weekday::Friday},
        {Weekday::Tuesday, Weekday::Thursday}
    };
    const std::vector<Course::WeeklyMeetingDayPattern> singleDayPatterns{
        {Weekday::Monday},
        {Weekday::Tuesday},
        {Weekday::Wednesday},
        {Weekday::Thursday},
        {Weekday::Friday}
    };

    const auto theseus = Course::fromNames("E4", "Theseus");
    const auto athena = Course::fromNames("E5", "Athena");
    const auto standardE5 = Course::fromNames("E5", "Zeus");
    const auto standardE6 = Course::fromNames("E6", "Hera");
    const auto songsE6 = Course::fromNames("E6", "Song's");
    const auto standardM1 = Course::fromNames("M1", "Elephantus");
    const auto songsM1 = Course::fromNames("M1", "Song's");
    const auto songsM3 = Course::fromNames("M3", "Song's");
    QVERIFY(theseus.has_value());
    QVERIFY(athena.has_value());
    QVERIFY(standardE5.has_value());
    QVERIFY(standardE6.has_value());
    QVERIFY(songsE6.has_value());
    QVERIFY(standardM1.has_value());
    QVERIFY(songsM1.has_value());
    QVERIFY(songsM3.has_value());

    const auto pairedRule = theseus->weeklyMeetingDayRule();
    QVERIFY(pairedRule.has_value());
    QVERIFY(
        pairedRule->kind()
        == Course::WeeklyMeetingDayRuleKind::PairedWeekdays
        );
    QVERIFY(pairedRule->allowedPatterns() == pairedPatterns);
    QVERIFY(pairedRule->allows({Weekday::Monday, Weekday::Wednesday}));
    QVERIFY(pairedRule->allows({Weekday::Tuesday, Weekday::Thursday}));
    QVERIFY(!pairedRule->allows({Weekday::Monday}));

    const auto athenaRule = athena->weeklyMeetingDayRule();
    QVERIFY(athenaRule.has_value());
    QVERIFY(
        athenaRule->kind()
        == Course::WeeklyMeetingDayRuleKind::ThreeDayOrTuesdayThursday
        );
    QVERIFY(athenaRule->allowedPatterns() == threeDayPatterns);
    QVERIFY(athenaRule->allows(
        {Weekday::Monday, Weekday::Wednesday, Weekday::Friday}
        ));
    QVERIFY(athenaRule->allows({Weekday::Tuesday, Weekday::Thursday}));
    QVERIFY(!athenaRule->allows({Weekday::Monday, Weekday::Wednesday}));

    const auto standardE5Rule = standardE5->weeklyMeetingDayRule();
    QVERIFY(standardE5Rule.has_value());
    QVERIFY(
        standardE5Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::PairedWeekdays
        );
    QVERIFY(standardE5Rule->allowedPatterns() == pairedPatterns);
    QVERIFY(standardE5Rule->allows({Weekday::Monday, Weekday::Friday}));
    QVERIFY(!standardE5Rule->allows(
        {Weekday::Monday, Weekday::Wednesday, Weekday::Friday}
        ));

    const auto standardE6Rule = standardE6->weeklyMeetingDayRule();
    QVERIFY(standardE6Rule.has_value());
    QVERIFY(
        standardE6Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::SingleWeekday
        );
    QVERIFY(standardE6Rule->allowedPatterns() == singleDayPatterns);
    QVERIFY(standardE6Rule->allows({Weekday::Friday}));
    QVERIFY(!standardE6Rule->allows({Weekday::Monday, Weekday::Wednesday}));

    const auto songsE6Rule = songsE6->weeklyMeetingDayRule();
    QVERIFY(songsE6Rule.has_value());
    QVERIFY(
        songsE6Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::ThreeDayOrTuesdayThursday
        );
    QVERIFY(songsE6Rule->allowedPatterns() == threeDayPatterns);
    QVERIFY(songsE6Rule->allows(
        {Weekday::Monday, Weekday::Wednesday, Weekday::Friday}
        ));

    const auto standardM1Rule = standardM1->weeklyMeetingDayRule();
    QVERIFY(standardM1Rule.has_value());
    QVERIFY(
        standardM1Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::SingleWeekday
        );
    QVERIFY(standardM1Rule->allowedPatterns() == singleDayPatterns);
    QVERIFY(standardM1Rule->allows({Weekday::Tuesday}));
    QVERIFY(!standardM1Rule->allows({Weekday::Monday, Weekday::Friday}));

    const auto songsM1Rule = songsM1->weeklyMeetingDayRule();
    QVERIFY(songsM1Rule.has_value());
    QVERIFY(
        songsM1Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::PairedWeekdays
        );
    QVERIFY(songsM1Rule->allowedPatterns() == pairedPatterns);
    QVERIFY(songsM1Rule->allows({Weekday::Wednesday, Weekday::Friday}));

    const auto songsM3Rule = songsM3->weeklyMeetingDayRule();
    QVERIFY(songsM3Rule.has_value());
    QVERIFY(
        songsM3Rule->kind()
        == Course::WeeklyMeetingDayRuleKind::PairedWeekdays
        );
    QVERIFY(songsM3Rule->allowedPatterns() == pairedPatterns);
    QVERIFY(songsM3Rule->allows({Weekday::Monday, Weekday::Friday}));

    QVERIFY(!Course::weeklyMeetingDayRuleFor(
        CourseGradeBand::M3,
        CourseLevelCategory::Standard
        ).has_value());
}

void NextDomainContractTests::weeklyMeetingDayRulesRejectInvalidPatterns()
{
    const auto rule = Course::weeklyMeetingDayRuleFor(
        CourseGradeBand::E4,
        CourseLevelCategory::Standard
        );
    QVERIFY(rule.has_value());

    QVERIFY(rule->allows({Weekday::Wednesday, Weekday::Monday}));
    QVERIFY(!rule->allows({Weekday::Monday, Weekday::Monday}));
    QVERIFY(!rule->allows({Weekday::Monday, Weekday::Saturday}));
    QVERIFY(!rule->allows({Weekday::Sunday}));
    QVERIFY(!rule->allows({static_cast<Weekday>(7)}));
    QVERIFY(!rule->allows({}));
}

void NextDomainContractTests::
    weeklyMeetingDayRulesRejectUnknownCourseClassificationValues()
{
    constexpr std::array<CourseGradeBand, 7> declaredGrades{
        CourseGradeBand::E4,
        CourseGradeBand::E5,
        CourseGradeBand::E6,
        CourseGradeBand::M1,
        CourseGradeBand::M2,
        CourseGradeBand::M3,
        CourseGradeBand::Other
    };
    constexpr std::array<CourseLevelCategory, 3> declaredLevels{
        CourseLevelCategory::Standard,
        CourseLevelCategory::Athena,
        CourseLevelCategory::Songs
    };
    constexpr CourseGradeBand unknownGrade =
        static_cast<CourseGradeBand>(99);
    constexpr CourseLevelCategory unknownLevel =
        static_cast<CourseLevelCategory>(99);

    for (const CourseLevelCategory level : declaredLevels)
    {
        QVERIFY(!Course::weeklyMeetingDayRuleFor(
            unknownGrade,
            level
            ).has_value());
        QVERIFY(!Course::weeklyMeetingDayRuleFor(
            CourseGradeBand::Other,
            level
            ).has_value());
    }

    for (const CourseGradeBand grade : declaredGrades)
    {
        QVERIFY(!Course::weeklyMeetingDayRuleFor(
            grade,
            unknownLevel
            ).has_value());
    }
}

void NextDomainContractTests::
koreanTeacherKeysKeepEveryAcceptedRangeAndBoundary()
{
    const std::u16string accepted{
        u'\u1100', u'\u1101', u'\u11ff',
        u'\u3130', u'\u3131', u'\u318f',
        u'\ua960', u'\ua961', u'\ua97f',
        u'\uac00', u'\uac01', u'\ud7af',
        u'\ud7b0', u'\ud7b1', u'\ud7ff'
    };
    const KoreanTeacherKey key = KoreanTeacherKey::fromName(accepted);
    QVERIFY(key.value() == accepted);

    for (const char16_t codeUnit : accepted)
    {
        QVERIFY(KoreanTeacherKey::isHangulCodeUnit(codeUnit));
    }

    const std::u16string rejected{
        u'\u10ff', u'\u1200',
        u'\u312f', u'\u3190',
        u'\ua95f', u'\ua980',
        u'\uabff', static_cast<char16_t>(0xd800), u'\ue000'
    };
    const KoreanTeacherKey empty = KoreanTeacherKey::fromName(rejected);
    QVERIFY(empty.empty());
    QVERIFY(empty.value().empty());
    for (const char16_t codeUnit : rejected)
    {
        QVERIFY(!KoreanTeacherKey::isHangulCodeUnit(codeUnit));
    }
}

void NextDomainContractTests::
koreanTeacherKeysDiscardOtherCodeUnitsWithoutNormalization()
{
    const KoreanTeacherKey mixed = KoreanTeacherKey::fromName(
        u"A\u1100-\u3131 \uac00\ud7ffZ"
        );
    QVERIFY(mixed.value() == u"\u1100\u3131\uac00\ud7ff");

    const KoreanTeacherKey composed = KoreanTeacherKey::fromName(u"\ud55c");
    const KoreanTeacherKey decomposed =
        KoreanTeacherKey::fromName(u"\u1112\u1161\u11ab");
    QVERIFY(composed.value() == u"\ud55c");
    QVERIFY(decomposed.value() == u"\u1112\u1161\u11ab");
    QVERIFY(composed != decomposed);

    const KoreanTeacherKey compatibility =
        KoreanTeacherKey::fromName(u"\u3131");
    const KoreanTeacherKey leadingJamo =
        KoreanTeacherKey::fromName(u"\u1100");
    QVERIFY(compatibility.value() == u"\u3131");
    QVERIFY(leadingJamo.value() == u"\u1100");
    QVERIFY(compatibility != leadingJamo);
}

void NextDomainContractTests::koreanTeacherKeysExposeEmptyAndValueSemantics()
{
    const KoreanTeacherKey empty = KoreanTeacherKey::fromName(u"A 1");
    QVERIFY(empty.empty());
    QVERIFY(empty.value().empty());

    const KoreanTeacherKey first = KoreanTeacherKey::fromName(u"A\uac00");
    const KoreanTeacherKey sameValue = KoreanTeacherKey::fromName(u"\uac00");
    const KoreanTeacherKey different = KoreanTeacherKey::fromName(u"\uac01");
    QVERIFY(!first.empty());
    QVERIFY(first.value() == u"\uac00");
    QVERIFY(first == sameValue);
    QVERIFY(first != different);
}

void NextDomainContractTests::studentNamePairsRequireBothNames()
{
    QVERIFY(!StudentNamePair::fromNames(u"", u"Korean").has_value());
    QVERIFY(!StudentNamePair::fromNames(u"English", u"").has_value());
    QVERIFY(!StudentNamePair::fromNames(u"", u"").has_value());

    const auto pair = StudentNamePair::fromNames(u"English", u"Korean");
    QVERIFY(pair.has_value());
    QVERIFY(pair->englishName() == u"English");
    QVERIFY(pair->koreanName() == u"Korean");
}

void NextDomainContractTests::studentNamePairsCompareExactUtf16PartsAndOrder()
{
    const auto first = StudentNamePair::fromNames(
        u"Alex",
        u"\uae40\ubbfc\uc9c0"
        );
    const auto same = StudentNamePair::fromNames(
        u"Alex",
        u"\uae40\ubbfc\uc9c0"
        );
    const auto splitPair = StudentNamePair::fromNames(u"Alex Kim", u"Lee");
    const auto differentCase = StudentNamePair::fromNames(u"alex", u"Kim");
    const auto differentWhitespace =
        StudentNamePair::fromNames(u"Al  ex", u"Kim");
    const auto supplementary =
        StudentNamePair::fromNames(
            u"Alex\U0001f600",
            u"\uae40\ubbfc\uc9c0"
            );
    const auto decomposedKorean = StudentNamePair::fromNames(
        u"Alex",
        u"\u1100\u1161\u1106\u1175\u110c\u1175"
        );

    QVERIFY(first.has_value());
    QVERIFY(same.has_value());
    QVERIFY(splitPair.has_value());
    QVERIFY(differentCase.has_value());
    QVERIFY(differentWhitespace.has_value());
    QVERIFY(supplementary.has_value());
    QVERIFY(decomposedKorean.has_value());

    QVERIFY(*first == *same);
    QVERIFY(*first != *differentCase);
    QVERIFY(*first != *differentWhitespace);
    QVERIFY(*first != *supplementary);
    QVERIFY(*first != *decomposedKorean);
    QVERIFY(first->koreanName() == u"\uae40\ubbfc\uc9c0");
    QCOMPARE(supplementary->englishName().size(), std::size_t{6});

    QVERIFY(*first != *splitPair);

    const auto sameJoinedTextFirst =
        StudentNamePair::fromNames(u"Alex", u"KimLee");
    const auto sameJoinedTextSecond =
        StudentNamePair::fromNames(u"AlexKim", u"Lee");
    QVERIFY(sameJoinedTextFirst.has_value());
    QVERIFY(sameJoinedTextSecond.has_value());
    QVERIFY(*sameJoinedTextFirst != *sameJoinedTextSecond);

    const std::map<StudentNamePair, int> ordered{
        {*differentCase, 1},
        {*first, 2},
        {*splitPair, 3}
    };
    QCOMPARE(ordered.size(), std::size_t{3});
    QVERIFY(ordered.begin()->first == *first);
    QVERIFY(ordered.rbegin()->first == *differentCase);
}

void NextDomainContractTests::
studentNamePairsPreserveValuesAcrossRvalueOperations()
{
    const auto constructionValue = StudentNamePair::fromNames(
        u"Construction source",
        u"\uad6c\uc131 \uc18c\uc2a4"
        );
    QVERIFY(constructionValue.has_value());

    StudentNamePair constructionSource = *constructionValue;
    const StudentNamePair constructionSnapshot = constructionSource;
    const StudentNamePair constructed(std::move(constructionSource));
    QVERIFY(constructionSource == constructionSnapshot);
    QVERIFY(constructed == constructionSnapshot);

    const auto assignmentValue = StudentNamePair::fromNames(
        u"Assignment source",
        u"\ub300\uc785 \uc18c\uc2a4"
        );
    const auto initialDestinationValue = StudentNamePair::fromNames(
        u"Initial destination",
        u"\ucd08\uae30 \ub300\uc0c1"
        );
    QVERIFY(assignmentValue.has_value());
    QVERIFY(initialDestinationValue.has_value());

    StudentNamePair assignmentSource = *assignmentValue;
    const StudentNamePair assignmentSnapshot = assignmentSource;
    StudentNamePair assignmentDestination = *initialDestinationValue;
    assignmentDestination = std::move(assignmentSource);
    QVERIFY(assignmentSource == assignmentSnapshot);
    QVERIFY(assignmentDestination == assignmentSnapshot);
}

void NextDomainContractTests::
duplicateStudentNamePairGroupsAreExactAndDeterministic()
{
    const auto alex = StudentNamePair::fromNames(
        u"Alex",
        u"\uae40\ubbfc\uc9c0"
        );
    const auto jamie = StudentNamePair::fromNames(
        u"Jamie",
        u"\ubc15\uc9c0\ubbfc"
        );
    const auto differentCase = StudentNamePair::fromNames(
        u"alex",
        u"\uae40\ubbfc\uc9c0"
        );
    const auto delimiterPairFirst = StudentNamePair::fromNames(
        u"Alpha\u001fBeta",
        u"Gamma"
        );
    const auto delimiterPairSecond = StudentNamePair::fromNames(
        u"Alpha",
        u"Beta\u001fGamma"
        );
    QVERIFY(alex.has_value());
    QVERIFY(jamie.has_value());
    QVERIFY(differentCase.has_value());
    QVERIFY(delimiterPairFirst.has_value());
    QVERIFY(delimiterPairSecond.has_value());

    const std::vector<std::optional<StudentNamePair>> rows{
        *alex,
        *jamie,
        *alex,
        std::nullopt,
        *jamie,
        *differentCase,
        *delimiterPairFirst,
        *delimiterPairSecond,
        *delimiterPairFirst,
        *delimiterPairSecond
        };

    const auto groups = duplicateStudentNamePairGroups(rows);
    const std::vector<std::size_t> alexRows{0, 2};
    const std::vector<std::size_t> jamieRows{1, 4};
    const std::vector<std::size_t> delimiterPairFirstRows{6, 8};
    const std::vector<std::size_t> delimiterPairSecondRows{7, 9};
    QCOMPARE(groups.size(), std::size_t{4});
    QVERIFY(groups[0].namePair == *alex);
    QVERIFY(groups[0].rowIndexes == alexRows);
    QVERIFY(groups[1].namePair == *jamie);
    QVERIFY(groups[1].rowIndexes == jamieRows);
    QVERIFY(groups[2].namePair == *delimiterPairFirst);
    QVERIFY(groups[2].rowIndexes == delimiterPairFirstRows);
    QVERIFY(groups[3].namePair == *delimiterPairSecond);
    QVERIFY(groups[3].rowIndexes == delimiterPairSecondRows);

    const auto repeatedCall = duplicateStudentNamePairGroups(rows);
    QCOMPARE(repeatedCall.size(), groups.size());
    for (std::size_t index = 0; index < groups.size(); ++index)
    {
        QVERIFY(repeatedCall[index].namePair == groups[index].namePair);
        QVERIFY(repeatedCall[index].rowIndexes == groups[index].rowIndexes);
    }
}

void NextDomainContractTests::studentNamePairsKeepPartsDistinctAcrossDelimiter()
{
    const auto first = StudentNamePair::fromNames(
        u"Alpha\u001fBeta",
        u"Gamma"
        );
    const auto second = StudentNamePair::fromNames(
        u"Alpha",
        u"Beta\u001fGamma"
        );

    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    QVERIFY(*first != *second);

    const std::map<StudentNamePair, int> pairs{
        {*first, 1},
        {*second, 2}
    };
    QCOMPARE(pairs.size(), std::size_t{2});
}

void NextDomainContractTests::
teacherDisplayNamesSelectPreferredAndEnglishNames()
{
    const TeacherDisplayName preferred = TeacherDisplayName::select(
        u"Preferred Name",
        u"English Name",
        u"Romanized Name",
        u"\uad8c\uc601\uc0dd"
        );
    QVERIFY(preferred.value() == u"Preferred Name");

    const TeacherDisplayName english = TeacherDisplayName::select(
        u"",
        u"English Name",
        u"Romanized Name",
        u"\uad8c\uc601\uc0dd"
        );
    QVERIFY(english.value() == u"English Name");
}

void NextDomainContractTests::
teacherDisplayNamesSelectRomanizationAndKoreanNames()
{
    const TeacherDisplayName romanization = TeacherDisplayName::select(
        u"",
        u"",
        u"Romanized Name",
        u"\uad8c\uc601\uc0dd"
        );
    QVERIFY(romanization.value() == u"Romanized Name");

    const TeacherDisplayName korean = TeacherDisplayName::select(
        u"",
        u"",
        u"",
        u"\uad8c\uc601\uc0dd"
        );
    QVERIFY(korean.value() == u"\uad8c\uc601\uc0dd");
}

void NextDomainContractTests::teacherDisplayNamesKeepEmptyAndCopiedValues()
{
    const TeacherDisplayName empty = TeacherDisplayName::select(
        u"", u"", u"", u""
        );
    QVERIFY(empty.empty());
    QVERIFY(empty.value().empty());

    const std::u16string source = u"\uac00\ub098\U0001f600";
    const TeacherDisplayName selected = TeacherDisplayName::select(
        u"", source, u"", u""
        );
    const TeacherDisplayName copy = selected;
    QVERIFY(!selected.empty());
    QVERIFY(selected.value() == u"\uac00\ub098\U0001f600");
    QVERIFY(copy == selected);
    QVERIFY(copy.value() == u"\uac00\ub098\U0001f600");

    const TeacherDisplayName untrimmed = TeacherDisplayName::select(
        u"  ", u"English", u"", u""
        );
    QVERIFY(untrimmed.value() == u"  ");
}

QTEST_APPLESS_MAIN(NextDomainContractTests)

#include "next_domain_contract_tests.moc"

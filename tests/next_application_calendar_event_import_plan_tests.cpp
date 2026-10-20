#include "next/application/calendar_event_import_plan.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

static_assert(std::is_same_v<
    decltype(CalendarEventImportPlanRequest{}.initiallySkippedCount),
    CalendarEventImportCount
    >);
static_assert(std::is_same_v<
    decltype(CalendarEventImportPlan{}.skippedCount),
    CalendarEventImportCount
    >);
static_assert(std::is_same_v<
    decltype(CalendarEventImportPlan{}.acceptedCandidateIndices),
    std::vector<CalendarEventImportCandidateIndex>
    >);
static_assert(std::is_same_v<
    typename decltype(CalendarEventImportPlan{}.acceptedCandidateIndices)::value_type,
    CalendarEventImportCandidateIndex
    >);

namespace
{

CalendarEventImportSignature signature(std::u16string title)
{
    return CalendarEventImportSignature::fromNormalizedFields({
        .simplifiedTitle = std::move(title),
        .normalizedEventType = u"Other",
        .startDateIso = u"2026-09-23",
        .endDateIso = u"2026-09-23",
        .allDay = false,
        .normalizedTimeStatus = u"Unknown"
    });
}

} // namespace

class NextApplicationCalendarEventImportPlanTests final : public QObject
{
    Q_OBJECT

private slots:
    void countWrapsSignedIntWithoutImplicitConversions();
    void candidateIndexWrapsSizeTWithoutImplicitConversions();
    void emptyInputProducesEmptyPlan();
    void noCandidatesRetainParserSkippedCount();
    void existingSignaturesSkipMatchingCandidates();
    void duplicateCandidatesAreAcceptedOnlyOnceInInputOrder();
    void opaqueSignaturesUseExactStringEquality();
    void distinctLoneSurrogateKeysRemainDistinct();
};

void NextApplicationCalendarEventImportPlanTests::
countWrapsSignedIntWithoutImplicitConversions()
{
    static_assert(!std::is_convertible_v<int, CalendarEventImportCount>);
    static_assert(!std::is_convertible_v<CalendarEventImportCount, int>);
    static_assert(std::is_constructible_v<CalendarEventImportCount, int>);

    CalendarEventImportCount count;
    QCOMPARE(count.rawValue(), 0);
    QVERIFY(count == CalendarEventImportCount{});

    const CalendarEventImportCount minimum{
        std::numeric_limits<int>::min()
    };
    const CalendarEventImportCount maximum{
        std::numeric_limits<int>::max()
    };
    QCOMPARE(minimum.rawValue(), std::numeric_limits<int>::min());
    QCOMPARE(maximum.rawValue(), std::numeric_limits<int>::max());
    QVERIFY(minimum != maximum);

    CalendarEventImportCount& incremented = ++count;
    QVERIFY(&incremented == &count);
    QCOMPARE(count.rawValue(), 1);
    QVERIFY(count == CalendarEventImportCount{1});
}

void NextApplicationCalendarEventImportPlanTests::
candidateIndexWrapsSizeTWithoutImplicitConversions()
{
    static_assert(
        !std::is_convertible_v<std::size_t, CalendarEventImportCandidateIndex>
        );
    static_assert(
        !std::is_convertible_v<CalendarEventImportCandidateIndex, std::size_t>
        );
    static_assert(std::is_constructible_v<
        CalendarEventImportCandidateIndex,
        std::size_t
        >);

    const CalendarEventImportCandidateIndex zero;
    const CalendarEventImportCandidateIndex positive{6};

    QCOMPARE(zero.rawValue(), std::size_t(0));
    QCOMPARE(positive.rawValue(), std::size_t(6));
    QVERIFY(zero == CalendarEventImportCandidateIndex{});
    QVERIFY(positive == CalendarEventImportCandidateIndex{6});
    QVERIFY(zero != positive);
}

void NextApplicationCalendarEventImportPlanTests::emptyInputProducesEmptyPlan()
{
    const CalendarEventImportPlan plan =
        planCalendarEventImport(CalendarEventImportPlanRequest{});

    QVERIFY(plan.acceptedCandidateIndices.empty());
    QCOMPARE(plan.skippedCount.rawValue(), 0);
}

void NextApplicationCalendarEventImportPlanTests::
noCandidatesRetainParserSkippedCount()
{
    CalendarEventImportPlanRequest request;
    request.existingSignatures = {signature(u"already-present")};
    request.initiallySkippedCount = CalendarEventImportCount{6};

    const CalendarEventImportPlan plan =
        planCalendarEventImport(request);

    QVERIFY(plan.acceptedCandidateIndices.empty());
    QCOMPARE(plan.skippedCount.rawValue(), 6);
}

void NextApplicationCalendarEventImportPlanTests::
existingSignaturesSkipMatchingCandidates()
{
    CalendarEventImportPlanRequest request;
    request.existingSignatures = {
        signature(u"already-present"),
        signature(u"already-present")
    };
    request.candidateSignatures = {
        signature(u"already-present"),
        signature(u"new-event")
    };

    const CalendarEventImportPlan plan =
        planCalendarEventImport(request);

    const std::vector<CalendarEventImportCandidateIndex>
        expectedAcceptedIndices = {CalendarEventImportCandidateIndex{1}};
    QVERIFY(plan.acceptedCandidateIndices == expectedAcceptedIndices);
    QCOMPARE(plan.skippedCount.rawValue(), 1);
}

void NextApplicationCalendarEventImportPlanTests::
duplicateCandidatesAreAcceptedOnlyOnceInInputOrder()
{
    CalendarEventImportPlanRequest request;
    request.existingSignatures = {signature(u"existing")};
    request.candidateSignatures = {
        signature(u"first"),
        signature(u"existing"),
        signature(u"second"),
        signature(u"first"),
        signature(u"third"),
        signature(u"second")
    };
    request.initiallySkippedCount = CalendarEventImportCount{7};

    const CalendarEventImportPlan plan =
        planCalendarEventImport(request);

    const std::vector<CalendarEventImportCandidateIndex>
        expectedAcceptedIndices = {
            CalendarEventImportCandidateIndex{0},
            CalendarEventImportCandidateIndex{2},
            CalendarEventImportCandidateIndex{4}
        };
    QVERIFY(plan.acceptedCandidateIndices == expectedAcceptedIndices);
    // One match in the existing set plus two repeated candidate keys.
    QCOMPARE(plan.skippedCount.rawValue(), 10);
}

void NextApplicationCalendarEventImportPlanTests::
opaqueSignaturesUseExactStringEquality()
{
    CalendarEventImportPlanRequest request;
    request.existingSignatures = {signature(u"key|part"), signature(u"CASE")};
    request.candidateSignatures = {
        signature(u"key|part"),
        signature(u"key"),
        signature(u"case"),
        signature(u"CASE")
    };

    const CalendarEventImportPlan plan =
        planCalendarEventImport(request);

    const std::vector<CalendarEventImportCandidateIndex>
        expectedAcceptedIndices = {
            CalendarEventImportCandidateIndex{1},
            CalendarEventImportCandidateIndex{2}
        };
    QVERIFY(plan.acceptedCandidateIndices == expectedAcceptedIndices);
    QCOMPARE(plan.skippedCount.rawValue(), 2);
}

void NextApplicationCalendarEventImportPlanTests::
distinctLoneSurrogateKeysRemainDistinct()
{
    const CalendarEventImportSignature existingKey = signature(
        std::u16string(1, static_cast<char16_t>(0xD800))
        );
    const CalendarEventImportSignature distinctKey = signature(
        std::u16string(1, static_cast<char16_t>(0xD801))
        );

    CalendarEventImportPlanRequest request;
    request.existingSignatures = {existingKey};
    request.candidateSignatures = {existingKey, distinctKey};

    const CalendarEventImportPlan plan =
        planCalendarEventImport(request);

    const std::vector<CalendarEventImportCandidateIndex>
        expectedAcceptedIndices = {CalendarEventImportCandidateIndex{1}};
    QVERIFY(plan.acceptedCandidateIndices == expectedAcceptedIndices);
    QCOMPARE(plan.skippedCount.rawValue(), 1);
}

QTEST_APPLESS_MAIN(NextApplicationCalendarEventImportPlanTests)

#include "next_application_calendar_event_import_plan_tests.moc"

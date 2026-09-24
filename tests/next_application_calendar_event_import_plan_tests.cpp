#include "next/application/calendar_event_import_plan.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

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
    void emptyInputProducesEmptyPlan();
    void noCandidatesRetainParserSkippedCount();
    void existingSignaturesSkipMatchingCandidates();
    void duplicateCandidatesAreAcceptedOnlyOnceInInputOrder();
    void opaqueSignaturesUseExactStringEquality();
    void distinctLoneSurrogateKeysRemainDistinct();
};

void NextApplicationCalendarEventImportPlanTests::emptyInputProducesEmptyPlan()
{
    const CalendarEventImportPlan plan =
        planCalendarEventImport(CalendarEventImportPlanRequest{});

    QVERIFY(plan.acceptedCandidateIndices.empty());
    QCOMPARE(plan.skippedCount, 0);
}

void NextApplicationCalendarEventImportPlanTests::
noCandidatesRetainParserSkippedCount()
{
    CalendarEventImportPlanRequest request;
    request.existingSignatures = {signature(u"already-present")};
    request.initiallySkippedCount = 6;

    const CalendarEventImportPlan plan =
        planCalendarEventImport(request);

    QVERIFY(plan.acceptedCandidateIndices.empty());
    QCOMPARE(plan.skippedCount, 6);
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

    const std::vector<std::size_t> expectedAcceptedIndices = {1};
    QVERIFY(plan.acceptedCandidateIndices == expectedAcceptedIndices);
    QCOMPARE(plan.skippedCount, 1);
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
    request.initiallySkippedCount = 7;

    const CalendarEventImportPlan plan =
        planCalendarEventImport(request);

    const std::vector<std::size_t> expectedAcceptedIndices = {0, 2, 4};
    QVERIFY(plan.acceptedCandidateIndices == expectedAcceptedIndices);
    // One match in the existing set plus two repeated candidate keys.
    QCOMPARE(plan.skippedCount, 10);
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

    const std::vector<std::size_t> expectedAcceptedIndices = {1, 2};
    QVERIFY(plan.acceptedCandidateIndices == expectedAcceptedIndices);
    QCOMPARE(plan.skippedCount, 2);
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

    const std::vector<std::size_t> expectedAcceptedIndices = {1};
    QVERIFY(plan.acceptedCandidateIndices == expectedAcceptedIndices);
    QCOMPARE(plan.skippedCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationCalendarEventImportPlanTests)

#include "next_application_calendar_event_import_plan_tests.moc"

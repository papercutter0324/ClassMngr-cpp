#include "next/application/calendar_event_import_plan.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <vector>

using namespace ClassMngr::Next::Application;

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
    request.existingSignatures = {u"already-present"};
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
    request.existingSignatures = {u"already-present", u"already-present"};
    request.candidateSignatures = {u"already-present", u"new-event"};

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
    request.existingSignatures = {u"existing"};
    request.candidateSignatures = {
        u"first",
        u"existing",
        u"second",
        u"first",
        u"third",
        u"second"
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
    request.existingSignatures = {u"key|part", u"CASE"};
    request.candidateSignatures = {
        u"key|part",
        u"key",
        u"case",
        u"CASE"
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
    const std::u16string existingKey(1, static_cast<char16_t>(0xD800));
    const std::u16string distinctKey(1, static_cast<char16_t>(0xD801));

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

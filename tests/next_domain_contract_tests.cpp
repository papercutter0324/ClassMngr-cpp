#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/domain/schedule_time.h"

#include <QtTest/QtTest>

#include <type_traits>

using namespace ClassMngr::Next::Domain;

class NextDomainContractTests final : public QObject
{
    Q_OBJECT

private slots:
    void typedIdentifiersRejectEmptyValues();
    void typedIdentifiersRemainDistinct();
    void resultCarriesValueOrStructuredError();
    void scheduleTimesAcceptWeekdayAndMinuteBoundaries();
    void scheduleTimesRejectInvalidDaysAndIntervals();
    void scheduleTimesHaveValueAndOverlapSemantics();
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

QTEST_APPLESS_MAIN(NextDomainContractTests)

#include "next_domain_contract_tests.moc"

#include "next/application/schedule_slot_state_read_query.h"
#include "next/application/schedule_testing_assignment_read_query.h"

#include <QtTest/QtTest>

#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

static_assert(std::is_same_v<
    decltype(ScheduleSlotStateReadRow::identity),
    ScheduleSlotStateReadIdentity
    >);
static_assert(!std::is_same_v<
    ScheduleSlotStateReadIdentity,
    ScheduleTestingAssignmentKey
    >);
static_assert(!std::is_convertible_v<
    ScheduleSlotStateReadIdentity,
    ScheduleTestingAssignmentKey
    >);
static_assert(!std::is_convertible_v<
    ScheduleTestingAssignmentKey,
    ScheduleSlotStateReadIdentity
    >);
static_assert(!std::is_convertible_v<
    std::u16string,
    ScheduleSlotStateReadIdentity
    >);
static_assert(!std::is_convertible_v<
    ScheduleSlotStateReadIdentity,
    std::u16string
    >);

class RecordingScheduleSlotStateReadPort final
    : public ScheduleSlotStateReadPort
{
public:
    [[nodiscard]] ScheduleSlotStateReadResult readSlotStates(
        const ScheduleSlotStateReadQuery& query
        ) const override
    {
        ++readCount;
        lastQuery = query;
        return result;
    }

    mutable int readCount = 0;
    mutable ScheduleSlotStateReadQuery lastQuery;
    ScheduleSlotStateReadResult result =
        ScheduleSlotStateReadResult::success({});
};

}

class NextApplicationScheduleSlotStateReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void identityPreservesExactPairValuesAndEquality();
    void preservesOrderedRawRowsIncludingUnknownValues();
    void acceptsAnEmptySuccessfulSnapshot();
    void propagatesStructuredReadFailures();
};

void NextApplicationScheduleSlotStateReadQueryTests::
identityPreservesExactPairValuesAndEquality()
{
    const ScheduleSlotStateReadIdentity empty;
    QVERIFY(empty.day().empty());
    QVERIFY(empty.startTime().empty());

    const ScheduleSlotStateReadIdentity exact(u" Monday ", u"09:05 ");
    const ScheduleSlotStateReadIdentity samePair(u" Monday ", u"09:05 ");
    const ScheduleSlotStateReadIdentity differentDay(u"Monday", u"09:05 ");
    const ScheduleSlotStateReadIdentity differentTime(u" Monday ", u"09:05");

    QCOMPARE(exact.day(), std::u16string(u" Monday "));
    QCOMPARE(exact.startTime(), std::u16string(u"09:05 "));
    QVERIFY(exact == samePair);
    QVERIFY(exact != differentDay);
    QVERIFY(exact != differentTime);
}

void NextApplicationScheduleSlotStateReadQueryTests::
preservesOrderedRawRowsIncludingUnknownValues()
{
    RecordingScheduleSlotStateReadPort port;
    const ScheduleSlotStateReadQuery query;
    const ScheduleSlotStateReadSnapshot expected{
        .rows = {
            {
                ScheduleSlotStateReadIdentity(u" 未知 day ", u"09:05 "),
                u" unknown state 🧭"
            },
            {
                ScheduleSlotStateReadIdentity(u"Monday", u"10:00"),
                u"lunch"
            },
            {
                ScheduleSlotStateReadIdentity(u"Monday", u"09:00"),
                u"malformed state"
            }
        }
    };
    port.result = ScheduleSlotStateReadResult::success(expected);

    const auto result = ScheduleSlotStateReadQueryHandler::execute(
        query,
        port
        );

    QVERIFY(result);
    QCOMPARE(port.readCount, 1);
    QVERIFY(port.lastQuery == query);
    QCOMPARE(result.value(), expected);
    QCOMPARE(result.value().rows[0].identity.day(),
             std::u16string(u" 未知 day "));
    QCOMPARE(result.value().rows[0].identity.startTime(),
             std::u16string(u"09:05 "));
    QCOMPARE(result.value().rows[0].state,
             std::u16string(u" unknown state 🧭"));
    QCOMPARE(result.value().rows[1].identity.startTime(),
             std::u16string(u"10:00"));
    QCOMPARE(result.value().rows[2].identity.startTime(),
             std::u16string(u"09:00"));
}

void NextApplicationScheduleSlotStateReadQueryTests::
acceptsAnEmptySuccessfulSnapshot()
{
    RecordingScheduleSlotStateReadPort port;
    const auto result = ScheduleSlotStateReadQueryHandler::execute({}, port);

    QVERIFY(result);
    QVERIFY(result.value().rows.empty());
    QCOMPARE(port.readCount, 1);
}

void NextApplicationScheduleSlotStateReadQueryTests::
propagatesStructuredReadFailures()
{
    RecordingScheduleSlotStateReadPort port;
    const Domain::OperationError readError{
        .code = Domain::ErrorCode::Technical,
        .message = "slot-state repository failed",
        .recoverable = false
    };
    port.result = ScheduleSlotStateReadResult::failure(readError);

    const auto result = ScheduleSlotStateReadQueryHandler::execute({}, port);

    QVERIFY(!result);
    QVERIFY(result.error() == readError);
    QCOMPARE(port.readCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationScheduleSlotStateReadQueryTests)

#include "next_application_schedule_slot_state_read_query_tests.moc"

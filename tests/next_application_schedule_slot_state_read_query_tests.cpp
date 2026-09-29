#include "next/application/schedule_slot_state_read_query.h"

#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

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
    void preservesOrderedRawRowsIncludingUnknownValues();
    void acceptsAnEmptySuccessfulSnapshot();
    void propagatesStructuredReadFailures();
};

void NextApplicationScheduleSlotStateReadQueryTests::
preservesOrderedRawRowsIncludingUnknownValues()
{
    RecordingScheduleSlotStateReadPort port;
    const ScheduleSlotStateReadQuery query;
    const ScheduleSlotStateReadSnapshot expected{
        .rows = {
            {u" 未知 day ", u"09:05 ", u" unknown state 🧭"},
            {u"Monday", u"10:00", u"lunch"},
            {u"Monday", u"09:00", u"malformed state"}
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
    QCOMPARE(result.value().rows[0].day,
             std::u16string(u" 未知 day "));
    QCOMPARE(result.value().rows[0].startTime, std::u16string(u"09:05 "));
    QCOMPARE(result.value().rows[0].state,
             std::u16string(u" unknown state 🧭"));
    QCOMPARE(result.value().rows[1].startTime, std::u16string(u"10:00"));
    QCOMPARE(result.value().rows[2].startTime, std::u16string(u"09:00"));
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

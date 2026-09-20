#include "next/application/calendar_event_query_port.h"

#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

CalendarEventSummary testSummary()
{
    return {
        *CalendarEventId::fromString("event-1"),
        std::nullopt,
        std::nullopt,
        "Calendar event",
        "2026-07-10",
        "2026-07-11",
        std::string("09:00"),
        std::string("10:00"),
        {},
        {},
        0,
        false,
        "Meeting",
        "Timed",
        std::string("series-1")
    };
}

CalendarEventProjection testProjection()
{
    const auto result = CalendarEventProjection::create({{testSummary()}});
    Q_ASSERT(result);
    return result.value();
}

struct FakeState final
{
    int createCalls = 0;
    int rangeCalls = 0;
    int nextEventCalls = 0;
    std::optional<CalendarEventRangeRequest> lastRangeRequest;
    std::optional<CalendarEventNextEventRequest> lastNextEventRequest;
};

class FakePort final : public CalendarEventQueryPort
{
public:
    explicit FakePort(
        std::shared_ptr<FakeState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] CalendarEventProjectionResult loadRange(
        const CalendarEventRangeRequest& request
        ) override
    {
        ++m_state->rangeCalls;
        m_state->lastRangeRequest = request;
        return CalendarEventProjectionResult::success(testProjection());
    }

    [[nodiscard]] CalendarEventDateResult findNextEventDate(
        const CalendarEventNextEventRequest& request
        ) override
    {
        ++m_state->nextEventCalls;
        m_state->lastNextEventRequest = request;
        return CalendarEventDateResult::success(
            CalendarEventDate("2026-07-10")
            );
    }

private:
    std::shared_ptr<FakeState> m_state;
};

class FakeFactory final : public CalendarEventQueryPortFactory
{
public:
    explicit FakeFactory(
        std::shared_ptr<FakeState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] std::unique_ptr<CalendarEventQueryPort> create()
        const override
    {
        ++m_state->createCalls;
        return std::make_unique<FakePort>(m_state);
    }

private:
    std::shared_ptr<FakeState> m_state;
};

} // namespace

class NextApplicationCalendarEventQueryPortTests final : public QObject
{
    Q_OBJECT

private slots:
    void dateAndRequestsAreOwnedValueTypes();
    void portSurfaceReturnsTypedProjectionDateAndErrors();
    void factoryCreatesIndependentWorkerPorts();
};

void NextApplicationCalendarEventQueryPortTests::dateAndRequestsAreOwnedValueTypes()
{
    static_assert(std::is_copy_constructible_v<CalendarEventDate>);
    static_assert(std::is_copy_assignable_v<CalendarEventDate>);
    static_assert(std::is_copy_constructible_v<CalendarEventRangeRequest>);
    static_assert(std::is_copy_assignable_v<CalendarEventRangeRequest>);
    static_assert(std::is_copy_constructible_v<CalendarEventNextEventRequest>);
    static_assert(std::is_copy_assignable_v<CalendarEventNextEventRequest>);

    const auto start = CalendarEventDate::fromString("2026-07-01");
    const auto end = CalendarEventDate::fromString("2026-07-31");
    QVERIFY(start.has_value());
    QVERIFY(end.has_value());
    QVERIFY(!CalendarEventDate::fromString(" ").has_value());

    CalendarEventRangeRequest request{
        "calendar.db",
        *start,
        *end
    };
    const CalendarEventRangeRequest copy = request;
    request.databasePath.clear();
    request.startDate = CalendarEventDate();

    QCOMPARE(copy.databasePath, std::string("calendar.db"));
    QCOMPARE(copy.startDate.value(), std::string("2026-07-01"));
    QCOMPARE(copy.endDate.value(), std::string("2026-07-31"));
    const CalendarEventRangeRequest expected{
        "calendar.db",
        CalendarEventDate("2026-07-01"),
        CalendarEventDate("2026-07-31")
    };
    QVERIFY(copy == expected);
}

void NextApplicationCalendarEventQueryPortTests::
portSurfaceReturnsTypedProjectionDateAndErrors()
{
    using Port = CalendarEventQueryPort;
    using RangeResult = decltype(
        std::declval<Port&>().loadRange(
            std::declval<const CalendarEventRangeRequest&>()
            )
        );
    using NextResult = decltype(
        std::declval<Port&>().findNextEventDate(
            std::declval<const CalendarEventNextEventRequest&>()
            )
        );

    static_assert(std::is_same_v<RangeResult, CalendarEventProjectionResult>);
    static_assert(std::is_same_v<NextResult, CalendarEventDateResult>);
    static_assert(std::is_same_v<
        decltype(std::declval<CalendarEventProjectionResult>().error()),
        const CalendarEventQueryError&
        >);
    static_assert(!std::is_copy_constructible_v<Port>);

    auto state = std::make_shared<FakeState>();
    FakePort port(state);
    const auto projection = port.loadRange({
        "calendar.db",
        CalendarEventDate("2026-07-01"),
        CalendarEventDate("2026-07-31")
    });
    QVERIFY(projection);
    QCOMPARE(projection.value().eventCount(), std::size_t(1));

    const auto nextDate = port.findNextEventDate({
        "calendar.db",
        CalendarEventDate("2026-07-01")
    });
    QVERIFY(nextDate);
    QCOMPARE(nextDate.value().value(), std::string("2026-07-10"));

    const auto error = CalendarEventProjectionResult::failure({
        .code = ErrorCode::Technical,
        .message = "query failed",
        .recoverable = false
    });
    QVERIFY(!error);
    QCOMPARE(error.error().code, ErrorCode::Technical);
    QCOMPARE(error.error().message, std::string("query failed"));
}

void NextApplicationCalendarEventQueryPortTests::
factoryCreatesIndependentWorkerPorts()
{
    auto state = std::make_shared<FakeState>();
    FakeFactory factory(state);

    const auto first = factory.create();
    const auto second = factory.create();
    QVERIFY(first);
    QVERIFY(second);
    QVERIFY(first.get() != second.get());
    QCOMPARE(state->createCalls, 2);

    const auto firstResult = first->findNextEventDate({
        "first.db",
        CalendarEventDate("2026-07-01")
    });
    const auto secondResult = second->findNextEventDate({
        "second.db",
        CalendarEventDate("2026-08-01")
    });
    QVERIFY(firstResult);
    QVERIFY(secondResult);
    QCOMPARE(state->nextEventCalls, 2);
    QCOMPARE(
        state->lastNextEventRequest->databasePath,
        std::string("second.db")
        );
}

QTEST_APPLESS_MAIN(NextApplicationCalendarEventQueryPortTests)

#include "next_application_calendar_event_query_port_tests.moc"

#include "next/application/sub_prep_calendar_event_intervals_query.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <utility>
#include <optional>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

class FakeReadPort final : public SubPrepCalendarEventIntervalsReadPort
{
public:
    SubPrepCalendarEventIntervalsReadResult loadIntervals(
        const SubPrepCalendarEventIntervalsReadRequest& request
        ) override
    {
        ++calls;
        lastRequest = request;
        if (failure.has_value())
        {
            return SubPrepCalendarEventIntervalsReadResult::failure(*failure);
        }
        return SubPrepCalendarEventIntervalsReadResult::success(
            std::move(intervals)
            );
    }

    int calls = 0;
    SubPrepCalendarEventIntervalsReadRequest lastRequest;
    SubPrepCalendarEventIntervals intervals;
    std::optional<OperationError> failure;
};

} // namespace

class NextApplicationSubPrepCalendarEventIntervalsQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void contractContainsOnlyTheThreeRequiredIntervalValues();
    void filtersByTrimmedExactTypesAndKeepsFullInclusiveIntervals();
    void returnsEveryMatchingIntervalWithoutTheCalendarProjectionCeiling();
    void derivesCurrentAndFollowingCalendarYearWindow();
    void clampsWindowAtMaximumCalendarYear();
    void rejectsInvalidReferenceBeforeReading();
    void returnsReadFailureWithoutPartialIntervals();
};

void NextApplicationSubPrepCalendarEventIntervalsQueryTests::
contractContainsOnlyTheThreeRequiredIntervalValues()
{
    static_assert(std::is_same_v<
        decltype(SubPrepCalendarEventInterval::eventType),
        std::string
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepCalendarEventInterval::startDate),
        CalendarEventDate
        >);
    static_assert(std::is_same_v<
        decltype(SubPrepCalendarEventInterval::endDate),
        CalendarEventDate
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<SubPrepCalendarEventIntervalsReadPort&>()
                     .loadIntervals(
                         std::declval<
                             const SubPrepCalendarEventIntervalsReadRequest&
                             >()
                         )),
        SubPrepCalendarEventIntervalsReadResult
        >);

}

void NextApplicationSubPrepCalendarEventIntervalsQueryTests::
filtersByTrimmedExactTypesAndKeepsFullInclusiveIntervals()
{
    FakeReadPort readPort;
    readPort.intervals = {
        {"  Vacation\t", CalendarEventDate("2025-12-28"),
         CalendarEventDate("2026-07-08")},
        {"Holiday ", CalendarEventDate("2027-12-29"),
         CalendarEventDate("2028-01-05")},
        {"vacation", CalendarEventDate("2026-07-01"),
         CalendarEventDate("2026-07-10")},
        {"Other", CalendarEventDate("2026-07-01"),
         CalendarEventDate("2026-07-10")},
        {"Vacation", CalendarEventDate("bad"),
         CalendarEventDate("2026-07-10")},
        {"Holiday", CalendarEventDate("2026-07-12"),
         CalendarEventDate("2026-07-11")},
        {"Vacation", CalendarEventDate("2025-11-01"),
         CalendarEventDate("2025-11-15")},
        {"Holiday", CalendarEventDate("2028-01-01"),
         CalendarEventDate("2028-01-15")}
    };

    const SubPrepCalendarEventIntervalsRequest request{
        CalendarEventDate("2026-07-01")
    };
    const SubPrepCalendarEventIntervalsReadRequest expectedRange{
        CalendarEventDate("2026-01-01"),
        CalendarEventDate("2027-12-31")
    };
    const auto result = SubPrepCalendarEventIntervalsQuery(readPort).execute(
        request
        );

    QVERIFY(result);
    QCOMPARE(readPort.calls, 1);
    QCOMPARE(readPort.lastRequest, expectedRange);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value()[0].eventType, std::string("Vacation"));
    QCOMPARE(result.value()[0].startDate.value(), std::string("2025-12-28"));
    QCOMPARE(result.value()[0].endDate.value(), std::string("2026-07-08"));
    QCOMPARE(result.value()[1].eventType, std::string("Holiday"));
    QCOMPARE(result.value()[1].startDate.value(), std::string("2027-12-29"));
    QCOMPARE(result.value()[1].endDate.value(), std::string("2028-01-05"));
}

void NextApplicationSubPrepCalendarEventIntervalsQueryTests::
returnsEveryMatchingIntervalWithoutTheCalendarProjectionCeiling()
{
    FakeReadPort readPort;
    readPort.intervals.reserve(5'000);
    for (int index = 0; index < 5'000; ++index)
    {
        readPort.intervals.push_back({
            "Vacation",
            CalendarEventDate("2026-07-01"),
            CalendarEventDate("2026-07-29")
        });
    }

    const auto result = SubPrepCalendarEventIntervalsQuery(readPort).execute({
        CalendarEventDate("2026-07-01")
    });

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(5'000));
    QCOMPARE(readPort.calls, 1);
}

void NextApplicationSubPrepCalendarEventIntervalsQueryTests::
derivesCurrentAndFollowingCalendarYearWindow()
{
    const auto window =
        SubPrepCalendarEventIntervalsQueryDetail::readRequestForReferenceDate(
            CalendarEventDate("2026-07-01")
            );

    QVERIFY(window.has_value());
    QCOMPARE(window->startDate.value(), std::string("2026-01-01"));
    QCOMPARE(window->endDate.value(), std::string("2027-12-31"));
}

void NextApplicationSubPrepCalendarEventIntervalsQueryTests::
clampsWindowAtMaximumCalendarYear()
{
    const auto window =
        SubPrepCalendarEventIntervalsQueryDetail::readRequestForReferenceDate(
            CalendarEventDate("9999-12-31")
            );

    QVERIFY(window.has_value());
    QCOMPARE(window->startDate.value(), std::string("9999-01-01"));
    QCOMPARE(window->endDate.value(), std::string("9999-12-31"));
}

void NextApplicationSubPrepCalendarEventIntervalsQueryTests::
rejectsInvalidReferenceBeforeReading()
{
    FakeReadPort readPort;
    const auto invalid = SubPrepCalendarEventIntervalsQuery(readPort).execute({
        CalendarEventDate("2026-02-30")
    });
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, ErrorCode::InvalidInput);
    QCOMPARE(readPort.calls, 0);
}

void NextApplicationSubPrepCalendarEventIntervalsQueryTests::
returnsReadFailureWithoutPartialIntervals()
{
    FakeReadPort readPort;
    readPort.failure = OperationError{
        .code = ErrorCode::Technical,
        .message = "read failed",
        .recoverable = false
    };

    const auto result = SubPrepCalendarEventIntervalsQuery(readPort).execute({
        CalendarEventDate("2026-07-01")
    });

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Technical);
    QCOMPARE(result.error().message, std::string("read failed"));
}

QTEST_APPLESS_MAIN(NextApplicationSubPrepCalendarEventIntervalsQueryTests)

#include "next_application_sub_prep_calendar_event_intervals_query_tests.moc"

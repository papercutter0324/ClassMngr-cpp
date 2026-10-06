#include "next/application/schedule_testing_layout_clear_use_case.h"

#include <QtTest/QtTest>

#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class RecordingScheduleTestingLayoutClearPort final
    : public ScheduleTestingLayoutClearPort
{
public:
    [[nodiscard]] bool isAvailable() const override
    {
        ++availabilityCalls;
        return available;
    }

    [[nodiscard]] ScheduleTestingLayoutClearResult
        clearTestingLayout() const override
    {
        ++clearCalls;
        return response;
    }

    bool available = true;
    mutable int availabilityCalls = 0;
    mutable int clearCalls = 0;
    ScheduleTestingLayoutClearResult response =
        ScheduleTestingLayoutClearResult::success();
};

}

class NextApplicationScheduleTestingLayoutClearUseCaseTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void portUsesStructuredResultAndTypedAvailability();
    void useCaseForwardsAvailabilityWithoutClearing();
    void useCaseClearsOnceAndReturnsSuccess();
    void useCasePreservesStructuredPortFailure();
};

void NextApplicationScheduleTestingLayoutClearUseCaseTests::
portUsesStructuredResultAndTypedAvailability()
{
    using Port = ScheduleTestingLayoutClearPort;
    using ClearResult = decltype(
        std::declval<const Port&>().clearTestingLayout()
        );

    static_assert(std::is_same_v<
        ClearResult,
        ScheduleTestingLayoutClearResult
        >);
    static_assert(std::is_same_v<
        ScheduleTestingLayoutClearResult,
        Domain::Result<void>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const Port&>().isAvailable()),
        bool
        >);

    QVERIFY(true);
}

void NextApplicationScheduleTestingLayoutClearUseCaseTests::
useCaseForwardsAvailabilityWithoutClearing()
{
    RecordingScheduleTestingLayoutClearPort port;

    QVERIFY(ScheduleTestingLayoutClearUseCase::isAvailable(port));
    QCOMPARE(port.availabilityCalls, 1);
    QCOMPARE(port.clearCalls, 0);

    port.available = false;
    QVERIFY(!ScheduleTestingLayoutClearUseCase::isAvailable(port));
    QCOMPARE(port.availabilityCalls, 2);
    QCOMPARE(port.clearCalls, 0);
}

void NextApplicationScheduleTestingLayoutClearUseCaseTests::
useCaseClearsOnceAndReturnsSuccess()
{
    RecordingScheduleTestingLayoutClearPort port;

    const ScheduleTestingLayoutClearResult result =
        ScheduleTestingLayoutClearUseCase::execute(port);

    QVERIFY(result);
    QCOMPARE(port.clearCalls, 1);
    QCOMPARE(port.availabilityCalls, 0);
}

void NextApplicationScheduleTestingLayoutClearUseCaseTests::
useCasePreservesStructuredPortFailure()
{
    RecordingScheduleTestingLayoutClearPort port;
    port.response = ScheduleTestingLayoutClearResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "injected testing layout clear failure",
        .recoverable = false
    });

    const ScheduleTestingLayoutClearResult result =
        ScheduleTestingLayoutClearUseCase::execute(port);

    QVERIFY(!result);
    QCOMPARE(port.clearCalls, 1);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(result.error().message,
             std::string("injected testing layout clear failure"));
    QVERIFY(!result.error().recoverable);
}

QTEST_GUILESS_MAIN(NextApplicationScheduleTestingLayoutClearUseCaseTests)

#include "next_application_schedule_testing_layout_clear_use_case_tests.moc"

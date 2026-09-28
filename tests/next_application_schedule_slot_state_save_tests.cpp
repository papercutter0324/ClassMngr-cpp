#include "next/application/schedule_slot_state_save_use_case.h"

#include <QtTest/QtTest>

#include <optional>

using namespace ClassMngr::Next;

namespace
{

class RecordingSavePort final
    : public Application::ScheduleSlotStateSavePort
{
public:
    [[nodiscard]] Application::ScheduleSlotStateSaveResult saveSlotState(
        const Application::ScheduleSlotStateSaveRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ScheduleSlotStateSaveRequest>
        lastRequest;
    Application::ScheduleSlotStateSaveResult result =
        Application::ScheduleSlotStateSaveResult::success();
};

Application::ScheduleSlotStateSaveRequest validRequest()
{
    return {
        .weekday = Application::ScheduleWeekday::Saturday,
        .startMinute = 9 * 60 + 5,
        .selectedState = Application::ScheduleSlotState::Lunch,
        .defaultState = Application::ScheduleSlotState::Essay
    };
}

}

class NextApplicationScheduleSlotStateSaveTests final : public QObject
{
    Q_OBJECT

private slots:
    void forwardsValidTypedChoiceToPort();
    void rejectsInvalidWeekdayAndStartMinuteWithoutPortCall();
    void rejectsUnknownStateValuesWithoutPortCall();
    void preservesPortFailure();
};

void NextApplicationScheduleSlotStateSaveTests::
forwardsValidTypedChoiceToPort()
{
    const Application::ScheduleSlotStateSaveRequest request = validRequest();
    RecordingSavePort port;

    const auto result =
        Application::ScheduleSlotStateSaveUseCase::execute(request, port);

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(*port.lastRequest, request);
}

void NextApplicationScheduleSlotStateSaveTests::
rejectsInvalidWeekdayAndStartMinuteWithoutPortCall()
{
    RecordingSavePort port;

    auto request = validRequest();
    request.weekday = static_cast<Application::ScheduleWeekday>(7);
    auto result =
        Application::ScheduleSlotStateSaveUseCase::execute(request, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request = validRequest();
    request.startMinute = -1;
    result = Application::ScheduleSlotStateSaveUseCase::execute(request, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request.startMinute = 24 * 60;
    result = Application::ScheduleSlotStateSaveUseCase::execute(request, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationScheduleSlotStateSaveTests::
rejectsUnknownStateValuesWithoutPortCall()
{
    RecordingSavePort port;

    auto request = validRequest();
    request.selectedState =
        static_cast<Application::ScheduleSlotState>(3);
    auto result =
        Application::ScheduleSlotStateSaveUseCase::execute(request, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request = validRequest();
    request.defaultState =
        static_cast<Application::ScheduleSlotState>(3);
    result = Application::ScheduleSlotStateSaveUseCase::execute(request, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationScheduleSlotStateSaveTests::preservesPortFailure()
{
    RecordingSavePort port;
    port.result = Application::ScheduleSlotStateSaveResult::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "slot state rejected",
        .recoverable = true
    });

    const auto result =
        Application::ScheduleSlotStateSaveUseCase::execute(
            validRequest(),
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("slot state rejected"));
    QVERIFY(result.error().recoverable);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationScheduleSlotStateSaveTests)

#include "next_application_schedule_slot_state_save_tests.moc"

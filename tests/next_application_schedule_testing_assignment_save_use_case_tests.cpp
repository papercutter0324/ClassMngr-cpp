#include "next/application/schedule_testing_assignment_save_use_case.h"

#include <QtTest/QtTest>

#include <optional>

using namespace ClassMngr::Next;

namespace
{

class RecordingScheduleTestingAssignmentSavePort final
    : public Application::ScheduleTestingAssignmentSavePort
{
public:
    [[nodiscard]] Application::ScheduleTestingAssignmentSaveResult
    saveTestingAssignment(
        const Application::ScheduleTestingAssignmentSaveRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<
        Application::ScheduleTestingAssignmentSaveRequest
        > lastRequest;
    Application::ScheduleTestingAssignmentSaveResult result =
        Application::ScheduleTestingAssignmentSaveResult::success();
};

Application::ScheduleTestingAssignmentSaveRequest validPlainRequest()
{
    return {
        .mutation =
            Application::ScheduleTestingAssignmentMutation::SavePlainTesting,
        .day = u"Monday",
        .startTime = u"09:00",
        .room = u"Room 4",
        .replaceExisting = false
    };
}

}

class NextApplicationScheduleTestingAssignmentSaveUseCaseTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void forwardsExactValuesForEachMutation();
    void rejectsInvalidMutationFieldsWithoutCallingPort();
    void preservesStructuredPortFailure();
};

void NextApplicationScheduleTestingAssignmentSaveUseCaseTests::
forwardsExactValuesForEachMutation()
{
    RecordingScheduleTestingAssignmentSavePort port;

    auto request = validPlainRequest();
    request.day = u" Monday ";
    request.startTime = u" 09:05 ";
    request.room = u"  Room 4  ";
    request.replaceExisting = true;

    auto result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(result);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.callCount, 1);
    QCOMPARE(port.lastRequest->mutation, request.mutation);
    QCOMPARE(port.lastRequest->day, std::u16string(u" Monday "));
    QCOMPARE(port.lastRequest->startTime, std::u16string(u" 09:05 "));
    QCOMPARE(port.lastRequest->room, std::u16string(u"  Room 4  "));
    QVERIFY(!port.lastRequest->classId);
    QVERIFY(port.lastRequest->replaceExisting);

    request = validPlainRequest();
    request.mutation =
        Application::ScheduleTestingAssignmentMutation::RemoveAssignment;
    request.day = u"Tuesday ";
    request.startTime = u"10:00 ";
    request.room = u"unused remove room";
    request.replaceExisting = true;
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(result);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.callCount, 2);
    QCOMPARE(port.lastRequest->mutation, request.mutation);
    QCOMPARE(port.lastRequest->day, std::u16string(u"Tuesday "));
    QCOMPARE(port.lastRequest->startTime, std::u16string(u"10:00 "));
    QCOMPARE(port.lastRequest->room, std::u16string(u"unused remove room"));
    QVERIFY(!port.lastRequest->classId);
    QVERIFY(port.lastRequest->replaceExisting);

    request = validPlainRequest();
    request.mutation =
        Application::ScheduleTestingAssignmentMutation::AssignTestingClass;
    request.day = u"Wednesday";
    request.startTime = u"11:00";
    request.room = u"unused class room";
    request.classId = Domain::ClassId::fromString("42");
    request.replaceExisting = true;
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(result);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.callCount, 3);
    QCOMPARE(port.lastRequest->mutation, request.mutation);
    QCOMPARE(port.lastRequest->day, std::u16string(u"Wednesday"));
    QCOMPARE(port.lastRequest->startTime, std::u16string(u"11:00"));
    QCOMPARE(port.lastRequest->room, std::u16string(u"unused class room"));
    QVERIFY(port.lastRequest->classId.has_value());
    QCOMPARE(port.lastRequest->classId->value(), std::string("42"));
    QVERIFY(port.lastRequest->replaceExisting);
}

void NextApplicationScheduleTestingAssignmentSaveUseCaseTests::
rejectsInvalidMutationFieldsWithoutCallingPort()
{
    RecordingScheduleTestingAssignmentSavePort port;
    auto request = validPlainRequest();

    request.day.clear();
    auto result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request = validPlainRequest();
    request.startTime.clear();
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request = validPlainRequest();
    request.mutation =
        Application::ScheduleTestingAssignmentMutation::AssignTestingClass;
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request.classId = Domain::ClassId::fromString("not-an-integer");
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request.classId = Domain::ClassId::fromString("0");
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request = validPlainRequest();
    request.classId = Domain::ClassId::fromString("42");
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request = validPlainRequest();
    request.mutation =
        Application::ScheduleTestingAssignmentMutation::RemoveAssignment;
    request.classId = Domain::ClassId::fromString("42");
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    request = validPlainRequest();
    request.mutation =
        static_cast<Application::ScheduleTestingAssignmentMutation>(99);
    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request,
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationScheduleTestingAssignmentSaveUseCaseTests::
preservesStructuredPortFailure()
{
    RecordingScheduleTestingAssignmentSavePort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Conflict,
        .message = "testing assignment conflict",
        .recoverable = true
    };
    port.result =
        Application::ScheduleTestingAssignmentSaveResult::failure(expected);

    const auto result = Application::ScheduleTestingAssignmentSaveUseCase::
        execute(validPlainRequest(), port);

    QVERIFY(!result);
    QCOMPARE(port.callCount, 1);
    QCOMPARE(result.error().code, expected.code);
    QCOMPARE(result.error().message, expected.message);
    QCOMPARE(result.error().recoverable, expected.recoverable);
}

QTEST_GUILESS_MAIN(
    NextApplicationScheduleTestingAssignmentSaveUseCaseTests
    )

#include "next_application_schedule_testing_assignment_save_use_case_tests.moc"

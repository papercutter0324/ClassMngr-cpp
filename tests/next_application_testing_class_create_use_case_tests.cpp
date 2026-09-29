#include "next/application/testing_class_create_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    const auto parsed = Domain::ClassId::fromString(value);
    if (!parsed)
    {
        qFatal("Test class ID must have a typed representation.");
    }
    return *parsed;
}

Domain::TeacherId teacherId(const std::string& value)
{
    const auto parsed = Domain::TeacherId::fromString(value);
    if (!parsed)
    {
        qFatal("Test teacher ID must have a typed representation.");
    }
    return *parsed;
}

Application::TestingClassCreateRequest request()
{
    return {
        .name = u"F146 Testing Lab",
        .grade = u"M2",
        .level = u"Song's",
        .room = u"Library 204",
        .teacherId = teacherId("17"),
        .classColor = u"#123456",
        .fontColor = u"#FEDCBA",
        .notes = u"Create details",
        .assignmentDay = std::u16string(u"monday"),
        .assignmentStartTime = std::u16string(u"16:00")
    };
}

class RecordingTestingClassCreatePort final
    : public Application::TestingClassCreatePort
{
public:
    [[nodiscard]] Application::TestingClassCreateResult createTestingClass(
        const Application::TestingClassCreateRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TestingClassCreateRequest> lastRequest;
    Application::TestingClassCreateResult result =
        Application::TestingClassCreateResult::success(classId("42"));
};

}

class NextApplicationTestingClassCreateUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMalformedTeacherIdsBeforePort();
    void rejectsHalfPresentAssignmentBeforePort();
    void forwardsAllFieldsAndOptionalAssignmentAndReturnsTypedId();
    void forwardsRequestWithoutOptionalTeacherOrAssignment();
    void preservesStructuredPortFailure();
};

void NextApplicationTestingClassCreateUseCaseTests::
rejectsMalformedTeacherIdsBeforePort()
{
    RecordingTestingClassCreatePort port;
    for (const std::string value : {"0", "-1", "017", "teacher-17", "2147483648"})
    {
        auto invalidRequest = request();
        invalidRequest.teacherId = teacherId(value);

        const auto result = Application::TestingClassCreateUseCase::execute(
            invalidRequest,
            port
            );

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassCreateUseCaseTests::
rejectsHalfPresentAssignmentBeforePort()
{
    RecordingTestingClassCreatePort port;
    auto dayOnly = request();
    dayOnly.assignmentStartTime.reset();
    auto timeOnly = request();
    timeOnly.assignmentDay.reset();

    for (const auto& invalidRequest : {dayOnly, timeOnly})
    {
        const auto result = Application::TestingClassCreateUseCase::execute(
            invalidRequest,
            port
            );

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassCreateUseCaseTests::
forwardsAllFieldsAndOptionalAssignmentAndReturnsTypedId()
{
    RecordingTestingClassCreatePort port;
    const auto expected = request();

    const auto result = Application::TestingClassCreateUseCase::execute(
        expected,
        port
        );

    QVERIFY(result);
    QCOMPARE(result.value(), classId("42"));
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    const auto& forwarded = *port.lastRequest;
    QCOMPARE(forwarded.name, expected.name);
    QCOMPARE(forwarded.grade, expected.grade);
    QCOMPARE(forwarded.level, expected.level);
    QCOMPARE(forwarded.room, expected.room);
    QVERIFY(forwarded.teacherId.has_value());
    QCOMPARE(*forwarded.teacherId, *expected.teacherId);
    QCOMPARE(forwarded.classColor, expected.classColor);
    QCOMPARE(forwarded.fontColor, expected.fontColor);
    QCOMPARE(forwarded.notes, expected.notes);
    QVERIFY(forwarded.assignmentDay.has_value());
    QVERIFY(forwarded.assignmentStartTime.has_value());
    QCOMPARE(*forwarded.assignmentDay, *expected.assignmentDay);
    QCOMPARE(*forwarded.assignmentStartTime, *expected.assignmentStartTime);
}

void NextApplicationTestingClassCreateUseCaseTests::
forwardsRequestWithoutOptionalTeacherOrAssignment()
{
    RecordingTestingClassCreatePort port;
    auto withoutOptionals = request();
    withoutOptionals.teacherId.reset();
    withoutOptionals.assignmentDay.reset();
    withoutOptionals.assignmentStartTime.reset();

    const auto result = Application::TestingClassCreateUseCase::execute(
        withoutOptionals,
        port
        );

    QVERIFY(result);
    QCOMPARE(result.value(), classId("42"));
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(!port.lastRequest->teacherId.has_value());
    QVERIFY(!port.lastRequest->assignmentDay.has_value());
    QVERIFY(!port.lastRequest->assignmentStartTime.has_value());
}

void NextApplicationTestingClassCreateUseCaseTests::
preservesStructuredPortFailure()
{
    RecordingTestingClassCreatePort port;
    port.result = Application::TestingClassCreateResult::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "slot already occupied",
        .recoverable = true
    });

    const auto result = Application::TestingClassCreateUseCase::execute(
        request(),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("slot already occupied"));
    QVERIFY(result.error().recoverable);
    QCOMPARE(port.callCount, 1);
}

QTEST_GUILESS_MAIN(NextApplicationTestingClassCreateUseCaseTests)

#include "next_application_testing_class_create_use_case_tests.moc"

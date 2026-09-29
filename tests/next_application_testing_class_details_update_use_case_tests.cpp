#include "next/application/testing_class_details_update_use_case.h"

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

Application::TestingClassDetailsUpdateRequest request(
    const std::string& classIdValue = "42",
    const std::optional<std::string>& teacherIdValue =
        std::optional<std::string>("17")
    )
{
    return {
        .classId = classId(classIdValue),
        .name = u"Testing Lab 실험",
        .grade = u"M2",
        .level = u"Song's",
        .room = u"Library 204",
        .teacherId = teacherIdValue
            ? std::optional<Domain::TeacherId>(teacherId(*teacherIdValue))
            : std::nullopt,
        .classColor = u"#123456",
        .fontColor = u"#FEDCBA",
        .notes = u"Keep exact notes 상세"
    };
}

class RecordingTestingClassDetailsUpdatePort final
    : public Application::TestingClassDetailsUpdatePort
{
public:
    [[nodiscard]] Application::TestingClassDetailsUpdateResult
    updateTestingClassDetails(
        const Application::TestingClassDetailsUpdateRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TestingClassDetailsUpdateRequest>
        lastRequest;
    Application::TestingClassDetailsUpdateResult result =
        Domain::Result<void>::success();
};

}

class NextApplicationTestingClassDetailsUpdateUseCaseTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMalformedClassIdsBeforePort();
    void rejectsMalformedTeacherIdsBeforePort();
    void forwardsEveryFieldAndPreservesSuccess();
    void forwardsAbsentTeacherId();
    void preservesStructuredPortFailure();
};

void NextApplicationTestingClassDetailsUpdateUseCaseTests::
rejectsMalformedClassIdsBeforePort()
{
    RecordingTestingClassDetailsUpdatePort port;
    for (const std::string value : {
             "0", "-1", "042", "+42", "42x", "2147483648"
         })
    {
        const auto result =
            Application::TestingClassDetailsUpdateUseCase::execute(
                request(value),
                port
                );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassDetailsUpdateUseCaseTests::
rejectsMalformedTeacherIdsBeforePort()
{
    RecordingTestingClassDetailsUpdatePort port;
    for (const std::string value : {
             "0", "-1", "017", "+17", "teacher-17", "2147483648"
         })
    {
        const auto result =
            Application::TestingClassDetailsUpdateUseCase::execute(
                request("42", value),
                port
                );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassDetailsUpdateUseCaseTests::
forwardsEveryFieldAndPreservesSuccess()
{
    RecordingTestingClassDetailsUpdatePort port;
    const auto expected = request("42", "17");

    const auto result =
        Application::TestingClassDetailsUpdateUseCase::execute(
            expected,
            port
            );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId, classId("42"));
    QCOMPARE(port.lastRequest->name, expected.name);
    QCOMPARE(port.lastRequest->grade, expected.grade);
    QCOMPARE(port.lastRequest->level, expected.level);
    QCOMPARE(port.lastRequest->room, expected.room);
    QVERIFY(port.lastRequest->teacherId.has_value());
    QCOMPARE(port.lastRequest->teacherId.value(), teacherId("17"));
    QCOMPARE(port.lastRequest->classColor, expected.classColor);
    QCOMPARE(port.lastRequest->fontColor, expected.fontColor);
    QCOMPARE(port.lastRequest->notes, expected.notes);
}

void NextApplicationTestingClassDetailsUpdateUseCaseTests::
forwardsAbsentTeacherId()
{
    RecordingTestingClassDetailsUpdatePort port;
    const auto expected = request("42", std::nullopt);

    const auto result =
        Application::TestingClassDetailsUpdateUseCase::execute(
            expected,
            port
            );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(!port.lastRequest->teacherId.has_value());
}

void NextApplicationTestingClassDetailsUpdateUseCaseTests::
preservesStructuredPortFailure()
{
    RecordingTestingClassDetailsUpdatePort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Conflict,
        .message = "testing class details update rejected",
        .recoverable = true
    };
    port.result = Domain::Result<void>::failure(expected);

    const auto result =
        Application::TestingClassDetailsUpdateUseCase::execute(
            request(),
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, expected.code);
    QCOMPARE(result.error().message, expected.message);
    QCOMPARE(result.error().recoverable, expected.recoverable);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
}

QTEST_APPLESS_MAIN(NextApplicationTestingClassDetailsUpdateUseCaseTests)

#include "next_application_testing_class_details_update_use_case_tests.moc"

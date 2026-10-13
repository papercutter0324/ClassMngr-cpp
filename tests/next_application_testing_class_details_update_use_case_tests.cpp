#include "next/application/testing_class_details_update_use_case.h"

#include <QtTest/QtTest>

#include <array>
#include <optional>
#include <string>
#include <string_view>

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
    void rejectsMissingRequiredFieldsInRepositoryOrderBeforePort();
    void forwardsEveryFieldAndPreservesSuccess();
    void forwardsWideTeacherIdWithoutChangingItsValue();
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

    const auto bothIdsMalformed =
        Application::TestingClassDetailsUpdateUseCase::execute(
            request("042", "017"),
            port
            );
    QVERIFY(!bothIdsMalformed);
    QCOMPARE(bothIdsMalformed.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        bothIdsMalformed.error().message,
        "Testing class ID must be a canonical positive integer."
        );
    QVERIFY(bothIdsMalformed.error().recoverable);
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassDetailsUpdateUseCaseTests::
rejectsMalformedTeacherIdsBeforePort()
{
    RecordingTestingClassDetailsUpdatePort port;
    for (const std::string value : {
             "0", "-1", "017", "+17", "teacher-17", "17x", "9223372036854775808"
         })
    {
        const auto result =
            Application::TestingClassDetailsUpdateUseCase::execute(
                request("42", value),
                port
                );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            "Testing class teacher ID must be a canonical positive integer."
            );
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassDetailsUpdateUseCaseTests::
rejectsMissingRequiredFieldsInRepositoryOrderBeforePort()
{
    using Request = Application::TestingClassDetailsUpdateRequest;
    struct RequiredField final
    {
        std::u16string Request::*value;
        const char* message;
    };
    constexpr std::array requiredFields{
        RequiredField{
            &Request::name,
            "Testing class name is required."
        },
        RequiredField{
            &Request::grade,
            "Testing class grade is required."
        },
        RequiredField{
            &Request::level,
            "Testing class level is required."
        },
        RequiredField{
            &Request::room,
            "Testing class room is required."
        }
    };
    constexpr std::u16string_view whitespace = u" \u3000";

    const auto setRequiredFields = [&](Request& value, std::u16string text)
    {
        for (const RequiredField& field : requiredFields)
        {
            value.*(field.value) = text;
        }
    };

    RecordingTestingClassDetailsUpdatePort port;
    Request invalid = request();
    setRequiredFields(invalid, std::u16string(whitespace));

    for (const RequiredField& field : requiredFields)
    {
        const auto result =
            Application::TestingClassDetailsUpdateUseCase::execute(
                invalid,
                port
                );

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QCOMPARE(result.error().message, field.message);
        QVERIFY(result.error().recoverable);
        QCOMPARE(port.callCount, 0);

        invalid.*(field.value) = u"Valid";
    }

    Request invalidClassId = request("042");
    setRequiredFields(invalidClassId, std::u16string(whitespace));
    const auto classIdResult =
        Application::TestingClassDetailsUpdateUseCase::execute(
            invalidClassId,
            port
            );
    QVERIFY(!classIdResult);
    QCOMPARE(classIdResult.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        classIdResult.error().message,
        "Testing class ID must be a canonical positive integer."
        );
    QVERIFY(classIdResult.error().recoverable);

    Request invalidTeacherId = request("42", "017");
    setRequiredFields(invalidTeacherId, std::u16string(whitespace));
    const auto teacherIdResult =
        Application::TestingClassDetailsUpdateUseCase::execute(
            invalidTeacherId,
            port
            );
    QVERIFY(!teacherIdResult);
    QCOMPARE(teacherIdResult.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(
        teacherIdResult.error().message,
        "Testing class teacher ID must be a canonical positive integer."
        );
    QVERIFY(teacherIdResult.error().recoverable);
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassDetailsUpdateUseCaseTests::
forwardsEveryFieldAndPreservesSuccess()
{
    RecordingTestingClassDetailsUpdatePort port;
    auto expected = request("42", "17");
    expected.name = u"  Testing Lab ì‹¤í—˜ \u3000";

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
forwardsWideTeacherIdWithoutChangingItsValue()
{
    RecordingTestingClassDetailsUpdatePort port;
    const auto expected = request("42", "2147483648");

    const auto result = Application::TestingClassDetailsUpdateUseCase::execute(
        expected, port
        );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId, classId("42"));
    QVERIFY(port.lastRequest->teacherId.has_value());
    QCOMPARE(*port.lastRequest->teacherId, teacherId("2147483648"));
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

#include "next/application/testing_class_create_use_case.h"

#include <QtTest/QtTest>

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

bool qtCaseInsensitiveWeekdayEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QString::fromStdU16String(std::u16string(left)).compare(
               QString::fromStdU16String(std::u16string(right)),
               Qt::CaseInsensitive
               ) == 0;
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

Application::TestingClassCreateResult executeCreate(
    const Application::TestingClassCreateRequest& request,
    const Application::TestingClassCreatePort& port
    )
{
    return Application::TestingClassCreateUseCase::execute(
        request,
        port,
        qtCaseInsensitiveWeekdayEquals
        );
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
    void rejectsMissingRequiredFieldsBeforePort();
    void rejectsMalformedAssignmentKeysAfterRequiredFieldsBeforePort();
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
        invalidRequest.name.clear();

        const auto result = executeCreate(invalidRequest, port);

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string(
                "Testing class teacher ID must be a canonical positive integer."
                )
            );
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
    dayOnly.name.clear();
    auto timeOnly = request();
    timeOnly.assignmentDay.reset();
    timeOnly.name.clear();

    for (const auto& invalidRequest : {dayOnly, timeOnly})
    {
        const auto result = executeCreate(invalidRequest, port);

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string(
                "Testing class assignment weekday and start time must be provided together."
                )
            );
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassCreateUseCaseTests::
rejectsMissingRequiredFieldsBeforePort()
{
    struct RequiredFieldCase final
    {
        const char* message;
        std::u16string Application::TestingClassCreateRequest::*member;
    };

    const RequiredFieldCase requiredFields[] = {
        {
            "Testing class name is required.",
            &Application::TestingClassCreateRequest::name
        },
        {
            "Testing class grade is required.",
            &Application::TestingClassCreateRequest::grade
        },
        {
            "Testing class level is required.",
            &Application::TestingClassCreateRequest::level
        },
        {
            "Testing class room is required.",
            &Application::TestingClassCreateRequest::room
        }
    };

    RecordingTestingClassCreatePort port;
    auto orderedInvalidRequest = request();
    orderedInvalidRequest.name.clear();
    orderedInvalidRequest.grade.clear();
    orderedInvalidRequest.level.clear();
    orderedInvalidRequest.room.clear();
    orderedInvalidRequest.assignmentDay = u"Funday";
    orderedInvalidRequest.assignmentStartTime = u"9:00";

    for (const auto& field : requiredFields)
    {
        const auto result = executeCreate(orderedInvalidRequest, port);

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QCOMPARE(result.error().message, std::string(field.message));
        QVERIFY(result.error().recoverable);
        orderedInvalidRequest.*(field.member) = u"present";
    }

    for (const auto& field : requiredFields)
    {
        auto whitespaceOnlyRequest = request();
        whitespaceOnlyRequest.*(field.member) = u"\u3000";

        const auto result = executeCreate(whitespaceOnlyRequest, port);

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QCOMPARE(result.error().message, std::string(field.message));
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassCreateUseCaseTests::
rejectsMalformedAssignmentKeysAfterRequiredFieldsBeforePort()
{
    RecordingTestingClassCreatePort port;
    auto invalidWeekday = request();
    invalidWeekday.assignmentDay = u"Funday";
    auto invalidTime = request();
    invalidTime.assignmentStartTime = u"9:05";

    for (const auto& invalidRequest : {invalidWeekday, invalidTime})
    {
        const auto result = executeCreate(invalidRequest, port);

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QCOMPARE(
            result.error().message,
            std::string(
                "A testing assignment requires a valid weekday and start time."
                )
            );
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTestingClassCreateUseCaseTests::
forwardsAllFieldsAndOptionalAssignmentAndReturnsTypedId()
{
    RecordingTestingClassCreatePort port;
    auto expected = request();
    expected.name = u" \tF146 Testing Lab\u3000";
    expected.assignmentDay = u" \tTue\u017fday\u3000";
    expected.assignmentStartTime = u" 16:00\t";

    const auto result = executeCreate(expected, port);

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

    const auto result = executeCreate(withoutOptionals, port);

    QVERIFY(result);
    QCOMPARE(result.value(), classId("42"));
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QVERIFY(!port.lastRequest->teacherId.has_value());
    QVERIFY(!port.lastRequest->assignmentDay.has_value());
    QVERIFY(!port.lastRequest->assignmentStartTime.has_value());

    auto emptyAssignment = request();
    emptyAssignment.assignmentDay = u" \u3000 ";
    emptyAssignment.assignmentStartTime = u"\t";
    const auto emptyAssignmentResult = executeCreate(emptyAssignment, port);

    QVERIFY(emptyAssignmentResult);
    QCOMPARE(port.callCount, 2);
    QVERIFY(port.lastRequest->assignmentDay.has_value());
    QVERIFY(port.lastRequest->assignmentStartTime.has_value());
    QCOMPARE(*port.lastRequest->assignmentDay, *emptyAssignment.assignmentDay);
    QCOMPARE(
        *port.lastRequest->assignmentStartTime,
        *emptyAssignment.assignmentStartTime
        );
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

    const auto result = executeCreate(request(), port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("slot already occupied"));
    QVERIFY(result.error().recoverable);
    QCOMPARE(port.callCount, 1);
}

QTEST_GUILESS_MAIN(NextApplicationTestingClassCreateUseCaseTests)

#include "next_application_testing_class_create_use_case_tests.moc"

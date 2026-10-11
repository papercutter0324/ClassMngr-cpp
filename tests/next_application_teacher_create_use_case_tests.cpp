#include "next/application/teacher_create_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

class RecordingTeacherCreatePort final : public Application::TeacherCreatePort
{
public:
    [[nodiscard]] Domain::Result<void> preflight() const override
    {
        ++preflightCallCount;
        if (events)
        {
            events->push_back("preflight");
        }
        return preflightResult;
    }

    [[nodiscard]] Application::TeacherCreateResult createTeacher(
        const Application::TeacherCreateRequest& request
        ) const override
    {
        ++createCallCount;
        lastRequest = request;
        if (events)
        {
            events->push_back("create");
        }
        return result;
    }

    mutable int preflightCallCount = 0;
    mutable int createCallCount = 0;
    mutable std::optional<Application::TeacherCreateRequest> lastRequest;
    mutable std::vector<std::string>* events = nullptr;
    Domain::Result<void> preflightResult = Domain::Result<void>::success();
    Application::TeacherCreateResult result =
        Application::TeacherCreateResult::success(
            *Domain::TeacherId::fromString("42")
            );
};

class RecordingTeacherCreateValidationPolicy final
    : public Application::TeacherCreateValidationPolicy
{
public:
    [[nodiscard]] Application::TeacherCreateValidationResult validate(
        const Domain::TeacherProfileFields& fields
        ) const override
    {
        ++callCount;
        lastFields = fields;
        if (events)
        {
            events->push_back("validate");
        }
        if (result)
        {
            return *result;
        }
        return Application::TeacherCreateValidationResult::success(fields);
    }

    mutable int callCount = 0;
    mutable std::optional<Domain::TeacherProfileFields> lastFields;
    mutable std::vector<std::string>* events = nullptr;
    std::optional<Application::TeacherCreateValidationResult> result;
};

Application::TeacherCreateRequest sampleRequest()
{
    Domain::TeacherProfileFields fields;
    fields.teacherEn = u"  Alex   Smith  ";
    fields.teacherKr = u"Alex";
    fields.preferredRomanization = u"Alex";
    fields.preferredName = u"Alex Smith";
    fields.roomNumber = u"Room 9";
    fields.birthday = u"02-29";
    fields.phoneNumber = u"010-1234-5678";
    fields.wifiName = u"Campus WiFi";
    fields.wifiPassword = u"secret";
    fields.internetType = u"WiFi";
    fields.zoomId = u"alex.zoom";
    fields.zoomPassword = u"zoom-secret";
    fields.projectionType = u"HDMI";
    fields.notes = u"Initial setup";
    return {.fields = std::move(fields)};
}

}

class NextApplicationTeacherCreateUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void validatesBeforePersistingNormalizedFields();
    void validationFailureStopsBeforePersistence();
    void preflightFailurePrecedesFieldValidation();
    void preservesPortFailure();
};

void NextApplicationTeacherCreateUseCaseTests::
validatesBeforePersistingNormalizedFields()
{
    RecordingTeacherCreatePort port;
    RecordingTeacherCreateValidationPolicy validation;
    std::vector<std::string> events;
    port.events = &events;
    validation.events = &events;

    Domain::TeacherProfileFields normalizedFields = sampleRequest().fields;
    normalizedFields.teacherEn = u"Alex Smith";
    validation.result = Application::TeacherCreateValidationResult::success(
        normalizedFields);
    const auto request = sampleRequest();

    const auto result = Application::TeacherCreateUseCase::execute(
        request, validation, port);

    QVERIFY(result);
    QCOMPARE(result.value().value(), std::string("42"));
    QCOMPARE(port.preflightCallCount, 1);
    QCOMPARE(validation.callCount, 1);
    QCOMPARE(port.createCallCount, 1);
    QVERIFY(validation.lastFields.has_value());
    QCOMPARE(*validation.lastFields, request.fields);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->fields, normalizedFields);
    QVERIFY(events == (std::vector<std::string>{
        "preflight", "validate", "create"}));
}

void NextApplicationTeacherCreateUseCaseTests::
validationFailureStopsBeforePersistence()
{
    RecordingTeacherCreatePort port;
    RecordingTeacherCreateValidationPolicy validation;
    validation.result = Application::TeacherCreateValidationResult::failure({
        .code = Domain::ErrorCode::Validation,
        .message = "Teacher validation failed: teacherEn: teacher.name.invalid",
        .recoverable = true
    });

    const auto result = Application::TeacherCreateUseCase::execute(
        sampleRequest(), validation, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        result.error().message,
        std::string("Teacher validation failed: teacherEn: teacher.name.invalid")
        );
    QVERIFY(result.error().recoverable);
    QCOMPARE(port.preflightCallCount, 1);
    QCOMPARE(validation.callCount, 1);
    QCOMPARE(port.createCallCount, 0);
}

void NextApplicationTeacherCreateUseCaseTests::
preflightFailurePrecedesFieldValidation()
{
    RecordingTeacherCreatePort port;
    port.preflightResult = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::NotFound,
        .message = "The active database session for teachers is unavailable.",
        .recoverable = true
    });
    RecordingTeacherCreateValidationPolicy validation;

    const auto result = Application::TeacherCreateUseCase::execute(
        sampleRequest(), validation, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(
        result.error().message,
        std::string("The active database session for teachers is unavailable.")
        );
    QVERIFY(result.error().recoverable);
    QCOMPARE(port.preflightCallCount, 1);
    QCOMPARE(validation.callCount, 0);
    QCOMPARE(port.createCallCount, 0);
}

void NextApplicationTeacherCreateUseCaseTests::preservesPortFailure()
{
    RecordingTeacherCreatePort port;
    port.result = Application::TeacherCreateResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "teacher persistence rejected",
        .recoverable = false
    });
    RecordingTeacherCreateValidationPolicy validation;

    const auto result = Application::TeacherCreateUseCase::execute(
        sampleRequest(), validation, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(result.error().message, std::string("teacher persistence rejected"));
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.preflightCallCount, 1);
    QCOMPARE(validation.callCount, 1);
    QCOMPARE(port.createCallCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationTeacherCreateUseCaseTests)

#include "next_application_teacher_create_use_case_tests.moc"

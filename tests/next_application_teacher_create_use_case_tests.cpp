#include "next/application/teacher_create_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

class RecordingTeacherCreatePort final : public Application::TeacherCreatePort
{
public:
    [[nodiscard]] Application::TeacherCreateResult createTeacher(
        const Application::TeacherCreateRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TeacherCreateRequest> lastRequest;
    Application::TeacherCreateResult result =
        Application::TeacherCreateResult::success(
            *Domain::TeacherId::fromString("42")
            );
};

Application::TeacherCreateRequest sampleRequest()
{
    Domain::TeacherProfileFields fields;
    fields.teacherEn = u"Alex Smith";
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
    void forwardsFieldsAndTypedId();
    void preservesPortFailure();
};

void NextApplicationTeacherCreateUseCaseTests::forwardsFieldsAndTypedId()
{
    RecordingTeacherCreatePort port;
    const auto request = sampleRequest();

    const auto result =
        Application::TeacherCreateUseCase::execute(request, port);

    QVERIFY(result);
    QCOMPARE(result.value().value(), std::string("42"));
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(*port.lastRequest, request);
}

void NextApplicationTeacherCreateUseCaseTests::preservesPortFailure()
{
    RecordingTeacherCreatePort port;
    port.result = Application::TeacherCreateResult::failure({
        .code = Domain::ErrorCode::Validation,
        .message = "teacher rejected",
        .recoverable = true
    });

    const auto result =
        Application::TeacherCreateUseCase::execute(sampleRequest(), port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(result.error().message, std::string("teacher rejected"));
    QVERIFY(result.error().recoverable);
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationTeacherCreateUseCaseTests)

#include "next_application_teacher_create_use_case_tests.moc"

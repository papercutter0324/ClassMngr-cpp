#include "next/application/teacher_delete_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

Domain::TeacherId teacherId(const std::string& value)
{
    const auto parsed = Domain::TeacherId::fromString(value);
    if (!parsed)
    {
        qFatal("Test teacher ID must have a typed representation.");
    }
    return *parsed;
}

class RecordingTeacherDeletePort final : public Application::TeacherDeletePort
{
public:
    [[nodiscard]] Application::TeacherDeleteResult deleteTeacher(
        const Application::TeacherDeleteRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TeacherDeleteRequest> lastRequest;
    Application::TeacherDeleteResult result =
        Application::TeacherDeleteResult::success();
};

}

class NextApplicationTeacherDeleteUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidTeacherIdsBeforePort();
    void forwardsValidTeacherIdAndReturnsSuccess();
    void preservesStructuredPortFailure();
};

void NextApplicationTeacherDeleteUseCaseTests::
rejectsInvalidTeacherIdsBeforePort()
{
    RecordingTeacherDeletePort port;
    for (const std::string value : {
             "0",
             "-1",
             "017",
             "teacher-17",
             "2147483648"
         })
    {
        const Application::TeacherDeleteRequest request{
            .teacherId = teacherId(value)
        };

        const auto result = Application::TeacherDeleteUseCase::execute(
            request,
            port
            );

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QVERIFY(result.error().recoverable);
    }

    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationTeacherDeleteUseCaseTests::
forwardsValidTeacherIdAndReturnsSuccess()
{
    RecordingTeacherDeletePort port;
    const Application::TeacherDeleteRequest request{
        .teacherId = teacherId("42")
    };

    const auto result = Application::TeacherDeleteUseCase::execute(
        request,
        port
        );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->teacherId, request.teacherId);
}

void NextApplicationTeacherDeleteUseCaseTests::
preservesStructuredPortFailure()
{
    RecordingTeacherDeletePort port;
    port.result = Application::TeacherDeleteResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "injected teacher delete failure",
        .recoverable = false
    });

    const auto result = Application::TeacherDeleteUseCase::execute(
        Application::TeacherDeleteRequest{
            .teacherId = teacherId("42")
        },
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(result.error().message,
             std::string("injected teacher delete failure"));
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 1);
}

QTEST_GUILESS_MAIN(NextApplicationTeacherDeleteUseCaseTests)

#include "next_application_teacher_delete_use_case_tests.moc"

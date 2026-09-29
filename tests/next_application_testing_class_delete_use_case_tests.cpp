#include "next/application/testing_class_delete_use_case.h"

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

class RecordingTestingClassDeletePort final
    : public Application::TestingClassDeletePort
{
public:
    [[nodiscard]] Application::TestingClassDeleteResult deleteTestingClass(
        const Application::TestingClassDeleteRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::TestingClassDeleteRequest> lastRequest;
    Application::TestingClassDeleteResult result =
        Application::TestingClassDeleteResult::success();
};

}

class NextApplicationTestingClassDeleteUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidClassIdsBeforePort();
    void forwardsValidClassIdAndReturnsSuccess();
    void preservesStructuredPortFailure();
};

void NextApplicationTestingClassDeleteUseCaseTests::
rejectsInvalidClassIdsBeforePort()
{
    RecordingTestingClassDeletePort port;
    for (const std::string value : {
             "0",
             "-1",
             "017",
             "class-17",
             "2147483648"
         })
    {
        const Application::TestingClassDeleteRequest request{
            .classId = classId(value)
        };

        const auto result = Application::TestingClassDeleteUseCase::execute(
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

void NextApplicationTestingClassDeleteUseCaseTests::
forwardsValidClassIdAndReturnsSuccess()
{
    RecordingTestingClassDeletePort port;
    const Application::TestingClassDeleteRequest request{
        .classId = classId("42")
    };

    const auto result = Application::TestingClassDeleteUseCase::execute(
        request,
        port
        );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId, request.classId);
}

void NextApplicationTestingClassDeleteUseCaseTests::
preservesStructuredPortFailure()
{
    RecordingTestingClassDeletePort port;
    port.result = Application::TestingClassDeleteResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "injected delete failure",
        .recoverable = false
    });

    const auto result = Application::TestingClassDeleteUseCase::execute(
        Application::TestingClassDeleteRequest{
            .classId = classId("42")
        },
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(result.error().message, std::string("injected delete failure"));
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 1);
}

QTEST_GUILESS_MAIN(NextApplicationTestingClassDeleteUseCaseTests)

#include "next_application_testing_class_delete_use_case_tests.moc"

#include "next/application/class_delete_use_case.h"

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

class RecordingClassDeletePort final : public Application::ClassDeletePort
{
public:
    [[nodiscard]] Application::ClassDeleteResult deleteClass(
        const Application::ClassDeleteRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassDeleteRequest> lastRequest;
    Application::ClassDeleteResult result =
        Application::ClassDeleteResult::success();
};

}

class NextApplicationClassDeleteUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidClassIdsBeforePort();
    void forwardsValidClassIdAndReturnsSuccess();
    void preservesStructuredPortFailure();
};

void NextApplicationClassDeleteUseCaseTests::
rejectsInvalidClassIdsBeforePort()
{
    RecordingClassDeletePort port;
    for (const std::string value : {
             "0",
             "-1",
             "017",
             "class-17",
             "2147483648"
         })
    {
        const Application::ClassDeleteRequest request{
            .classId = classId(value)
        };

        const auto result = Application::ClassDeleteUseCase::execute(
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

void NextApplicationClassDeleteUseCaseTests::
forwardsValidClassIdAndReturnsSuccess()
{
    RecordingClassDeletePort port;
    const Application::ClassDeleteRequest request{
        .classId = classId("42")
    };

    const auto result = Application::ClassDeleteUseCase::execute(
        request,
        port
        );

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId, request.classId);
}

void NextApplicationClassDeleteUseCaseTests::
preservesStructuredPortFailure()
{
    RecordingClassDeletePort port;
    port.result = Application::ClassDeleteResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "injected class delete failure",
        .recoverable = false
    });

    const auto result = Application::ClassDeleteUseCase::execute(
        Application::ClassDeleteRequest{
            .classId = classId("42")
        },
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(result.error().message,
             std::string("injected class delete failure"));
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 1);
}

QTEST_GUILESS_MAIN(NextApplicationClassDeleteUseCaseTests)

#include "next_application_class_delete_use_case_tests.moc"

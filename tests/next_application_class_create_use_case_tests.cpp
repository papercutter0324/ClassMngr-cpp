#include "next/application/class_create_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

class RecordingCreatePort final : public Application::ClassCreatePort
{
public:
    [[nodiscard]] Application::ClassCreateResult createClass(
        const Application::ClassCreateRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassCreateRequest> lastRequest;
    Application::ClassCreateResult result =
        Application::ClassCreateResult::success(
            *Domain::ClassId::fromString("42")
            );
};

}

class NextApplicationClassCreateUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void forwardsCreateRequestAndTypedId();
    void preservesPortFailure();
};

void NextApplicationClassCreateUseCaseTests::
forwardsCreateRequestAndTypedId()
{
    RecordingCreatePort port;
    const Application::ClassCreateRequest request{};

    const auto result =
        Application::ClassCreateUseCase::execute(request, port);

    QVERIFY(result);
    QCOMPARE(result.value().value(), std::string("42"));
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
}

void NextApplicationClassCreateUseCaseTests::preservesPortFailure()
{
    RecordingCreatePort port;
    port.result = Application::ClassCreateResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "create rejected",
        .recoverable = false
    });

    const auto result = Application::ClassCreateUseCase::execute({}, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(result.error().message, std::string("create rejected"));
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationClassCreateUseCaseTests)

#include "next_application_class_create_use_case_tests.moc"

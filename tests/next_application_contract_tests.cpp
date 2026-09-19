#include "next/application/workspace_use_case.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

WorkspaceSession testSession()
{
    return WorkspaceSession(
        *WorkspaceId::fromString("workspace-1"),
        WorkspaceLocation("C:/workspaces/one.tps")
        );
}

class FakeWorkspaceGateway final : public WorkspaceGateway
{
public:
    [[nodiscard]] Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        ) override
    {
        ++createCalls;
        lastCreatedLocation = request.location;
        return *createResponse;
    }

    [[nodiscard]] Result<WorkspaceSession> openWorkspace(
        const OpenWorkspaceRequest& request
        ) override
    {
        ++openCalls;
        lastOpenedLocation = request.location;
        return *openResponse;
    }

    [[nodiscard]] Result<void> closeWorkspace(
        const CloseWorkspaceRequest& request
        ) override
    {
        ++closeCalls;
        lastClosedSession = request.session;
        return *closeResponse;
    }

    int createCalls = 0;
    int openCalls = 0;
    int closeCalls = 0;
    WorkspaceLocation lastCreatedLocation;
    WorkspaceLocation lastOpenedLocation;
    std::optional<WorkspaceSession> lastClosedSession;
    std::optional<Result<WorkspaceSession>> createResponse;
    std::optional<Result<WorkspaceSession>> openResponse;
    std::optional<Result<void>> closeResponse;
};

}

class NextApplicationContractTests final : public QObject
{
    Q_OBJECT

private slots:
    void invalidCreateDoesNotInvokeGateway();
    void invalidOpenDoesNotInvokeGateway();
    void invalidCloseDoesNotInvokeGateway();
    void whitespaceOnlyCreateDoesNotInvokeGateway();
    void createSuccessReturnsCopyableSessionProjection();
    void openSuccessReturnsSessionProjection();
    void createFailurePreservesGatewayError();
    void openFailurePreservesGatewayError();
    void closeSuccessUsesExplicitSession();
    void closeFailurePreservesGatewayError();
};

void NextApplicationContractTests::invalidCreateDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation{}}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.createCalls, 0);
}

void NextApplicationContractTests::invalidOpenDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation{}}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.openCalls, 0);
}

void NextApplicationContractTests::invalidCloseDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);
    const WorkspaceSession invalidSession(
        *WorkspaceId::fromString("workspace-1"),
        WorkspaceLocation{}
        );

    const auto result = useCase.closeWorkspace(
        CloseWorkspaceRequest{invalidSession}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.closeCalls, 0);
}

void NextApplicationContractTests::whitespaceOnlyCreateDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.createWorkspace(
        CreateWorkspaceRequest{
            WorkspaceLocation(" \t\r\n")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.createCalls, 0);
}

void NextApplicationContractTests::createSuccessReturnsCopyableSessionProjection()
{
    static_assert(std::is_copy_constructible_v<WorkspaceSession>);
    static_assert(std::is_copy_assignable_v<WorkspaceSession>);

    FakeWorkspaceGateway gateway;
    const WorkspaceSession expectedSession = testSession();
    gateway.createResponse = Result<WorkspaceSession>::success(expectedSession);
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.createWorkspace(
        CreateWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/one.tps")
        }
        );

    QVERIFY(result);
    QCOMPARE(gateway.createCalls, 1);
    QVERIFY(gateway.lastCreatedLocation == expectedSession.location());
    QVERIFY(result.value() == expectedSession);
    QCOMPARE(result.value().workspaceId().value(), std::string("workspace-1"));
}

void NextApplicationContractTests::openSuccessReturnsSessionProjection()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceSession expectedSession = testSession();
    gateway.openResponse = Result<WorkspaceSession>::success(expectedSession);
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.openWorkspace(
        OpenWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/one.tps")
        }
        );

    QVERIFY(result);
    QCOMPARE(gateway.openCalls, 1);
    QVERIFY(gateway.lastOpenedLocation == expectedSession.location());
    QVERIFY(result.value() == expectedSession);
}

void NextApplicationContractTests::createFailurePreservesGatewayError()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError{
        .code = ErrorCode::Technical,
        .message = "Creating the workspace failed.",
        .recoverable = false
    };
    gateway.createResponse = Result<WorkspaceSession>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.createWorkspace(
        CreateWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/one.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(gateway.createCalls, 1);
    QVERIFY(result.error() == expectedError);
}

void NextApplicationContractTests::openFailurePreservesGatewayError()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError{
        .code = ErrorCode::NotFound,
        .message = "Workspace file was not found.",
        .recoverable = true
    };
    gateway.openResponse = Result<WorkspaceSession>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.openWorkspace(
        OpenWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/missing.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(gateway.openCalls, 1);
    QVERIFY(result.error() == expectedError);
}

void NextApplicationContractTests::closeSuccessUsesExplicitSession()
{
    FakeWorkspaceGateway gateway;
    gateway.closeResponse = Result<void>::success();
    const WorkspaceSession session = testSession();
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.closeWorkspace(
        CloseWorkspaceRequest{session}
        );

    QVERIFY(result);
    QCOMPARE(gateway.closeCalls, 1);
    QVERIFY(gateway.lastClosedSession.has_value());
    QVERIFY(*gateway.lastClosedSession == session);
}

void NextApplicationContractTests::closeFailurePreservesGatewayError()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError{
        .code = ErrorCode::Technical,
        .message = "Closing the workspace failed.",
        .recoverable = false
    };
    gateway.closeResponse = Result<void>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.closeWorkspace(
        CloseWorkspaceRequest{testSession()}
        );

    QVERIFY(!result);
    QCOMPARE(gateway.closeCalls, 1);
    QVERIFY(result.error() == expectedError);
}

QTEST_APPLESS_MAIN(NextApplicationContractTests)

#include "next_application_contract_tests.moc"

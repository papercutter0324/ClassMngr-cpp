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

    [[nodiscard]] Result<void> saveWorkspace(
        const SaveWorkspaceRequest& request
        ) override
    {
        ++saveCalls;
        lastSavedSession = request.session;
        return *saveResponse;
    }

    [[nodiscard]] Result<WorkspaceLocation> saveWorkspaceAs(
        const SaveWorkspaceAsRequest& request
        ) override
    {
        ++saveAsCalls;
        lastSaveAsSession = request.session;
        lastSaveAsDestination = request.destination;
        return *saveAsResponse;
    }

    [[nodiscard]] Result<WorkspaceLocation> exportWorkspace(
        const ExportWorkspaceRequest& request
        ) override
    {
        ++exportCalls;
        lastExportedSession = request.session;
        lastExportDestination = request.destination;
        return *exportResponse;
    }

    int createCalls = 0;
    int openCalls = 0;
    int closeCalls = 0;
    int saveCalls = 0;
    int saveAsCalls = 0;
    int exportCalls = 0;
    WorkspaceLocation lastCreatedLocation;
    WorkspaceLocation lastOpenedLocation;
    std::optional<WorkspaceSession> lastClosedSession;
    std::optional<WorkspaceSession> lastSavedSession;
    std::optional<WorkspaceSession> lastSaveAsSession;
    WorkspaceLocation lastSaveAsDestination;
    std::optional<WorkspaceSession> lastExportedSession;
    WorkspaceLocation lastExportDestination;
    std::optional<Result<WorkspaceSession>> createResponse;
    std::optional<Result<WorkspaceSession>> openResponse;
    std::optional<Result<void>> closeResponse;
    std::optional<Result<void>> saveResponse;
    std::optional<Result<WorkspaceLocation>> saveAsResponse;
    std::optional<Result<WorkspaceLocation>> exportResponse;
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
    void invalidSaveDoesNotInvokeGateway();
    void saveSuccessUsesExplicitSession();
    void saveFailurePreservesGatewayError();
    void invalidSaveAsSessionDoesNotInvokeGateway();
    void whitespaceOnlySaveAsDestinationDoesNotInvokeGateway();
    void saveAsSuccessUsesExplicitSessionAndDestination();
    void saveAsFailurePreservesGatewayError();
    void invalidExportDoesNotInvokeGateway();
    void whitespaceOnlyExportDestinationDoesNotInvokeGateway();
    void exportSuccessUsesExplicitSessionAndDestination();
    void exportFailurePreservesGatewayError();
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

void NextApplicationContractTests::invalidSaveDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);
    const WorkspaceSession invalidSession(
        *WorkspaceId::fromString("workspace-1"),
        WorkspaceLocation(" \t\r\n")
        );

    const auto result = useCase.saveWorkspace(
        SaveWorkspaceRequest{invalidSession}
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.saveCalls, 0);
}

void NextApplicationContractTests::saveSuccessUsesExplicitSession()
{
    FakeWorkspaceGateway gateway;
    gateway.saveResponse = Result<void>::success();
    const WorkspaceSession session = testSession();
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.saveWorkspace(
        SaveWorkspaceRequest{session}
        );

    QVERIFY(result);
    QCOMPARE(gateway.saveCalls, 1);
    QVERIFY(gateway.lastSavedSession.has_value());
    QVERIFY(*gateway.lastSavedSession == session);
}

void NextApplicationContractTests::saveFailurePreservesGatewayError()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError{
        .code = ErrorCode::Technical,
        .message = "Saving the workspace failed.",
        .recoverable = false
    };
    gateway.saveResponse = Result<void>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.saveWorkspace(
        SaveWorkspaceRequest{testSession()}
        );

    QVERIFY(!result);
    QCOMPARE(gateway.saveCalls, 1);
    QVERIFY(result.error() == expectedError);
}

void NextApplicationContractTests::invalidSaveAsSessionDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);
    const WorkspaceSession invalidSession(
        *WorkspaceId::fromString("workspace-1"),
        WorkspaceLocation(" \t\r\n")
        );

    const auto result = useCase.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            invalidSession,
            WorkspaceLocation("C:/workspaces/two.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.saveAsCalls, 0);
}

void NextApplicationContractTests::whitespaceOnlySaveAsDestinationDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            testSession(),
            WorkspaceLocation(" \t\r\n")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.saveAsCalls, 0);
}

void NextApplicationContractTests::saveAsSuccessUsesExplicitSessionAndDestination()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceLocation destination("C:/workspaces/two.tps");
    gateway.saveAsResponse = Result<WorkspaceLocation>::success(destination);
    const WorkspaceSession session = testSession();
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.saveWorkspaceAs(
        SaveWorkspaceAsRequest{session, destination}
        );

    QVERIFY(result);
    QCOMPARE(gateway.saveAsCalls, 1);
    QVERIFY(gateway.lastSaveAsSession.has_value());
    QVERIFY(*gateway.lastSaveAsSession == session);
    QVERIFY(gateway.lastSaveAsDestination == destination);
    QVERIFY(result.value() == destination);
}

void NextApplicationContractTests::saveAsFailurePreservesGatewayError()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError{
        .code = ErrorCode::Technical,
        .message = "Saving the workspace as the requested destination failed.",
        .recoverable = false
    };
    gateway.saveAsResponse = Result<WorkspaceLocation>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.saveWorkspaceAs(
        SaveWorkspaceAsRequest{
            testSession(),
            WorkspaceLocation("C:/workspaces/two.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(gateway.saveAsCalls, 1);
    QVERIFY(result.error() == expectedError);
}

void NextApplicationContractTests::invalidExportDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);
    const WorkspaceSession invalidSession(
        *WorkspaceId::fromString("workspace-1"),
        WorkspaceLocation{}
        );

    const auto result = useCase.exportWorkspace(
        ExportWorkspaceRequest{
            invalidSession,
            WorkspaceLocation("C:/exports/one.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.exportCalls, 0);
}

void NextApplicationContractTests::whitespaceOnlyExportDestinationDoesNotInvokeGateway()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.exportWorkspace(
        ExportWorkspaceRequest{
            testSession(),
            WorkspaceLocation(" \t\r\n")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(gateway.exportCalls, 0);
}

void NextApplicationContractTests::exportSuccessUsesExplicitSessionAndDestination()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceLocation destination("C:/exports/one.tps");
    gateway.exportResponse = Result<WorkspaceLocation>::success(destination);
    const WorkspaceSession session = testSession();
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.exportWorkspace(
        ExportWorkspaceRequest{session, destination}
        );

    QVERIFY(result);
    QCOMPARE(gateway.exportCalls, 1);
    QVERIFY(gateway.lastExportedSession.has_value());
    QVERIFY(*gateway.lastExportedSession == session);
    QVERIFY(gateway.lastExportDestination == destination);
    QVERIFY(result.value() == destination);
}

void NextApplicationContractTests::exportFailurePreservesGatewayError()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError{
        .code = ErrorCode::Technical,
        .message = "Exporting the workspace failed.",
        .recoverable = false
    };
    gateway.exportResponse = Result<WorkspaceLocation>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);

    const auto result = useCase.exportWorkspace(
        ExportWorkspaceRequest{
            testSession(),
            WorkspaceLocation("C:/exports/one.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(gateway.exportCalls, 1);
    QVERIFY(result.error() == expectedError);
}

QTEST_APPLESS_MAIN(NextApplicationContractTests)

#include "next_application_contract_tests.moc"

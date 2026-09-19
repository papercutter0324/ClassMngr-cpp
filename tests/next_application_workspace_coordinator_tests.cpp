#include "next/application/workspace_coordinator.h"

#include <QtTest/QtTest>

#include <optional>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

WorkspaceSession testSession(
    const char* id,
    const char* location
    )
{
    return WorkspaceSession(
        *WorkspaceId::fromString(id),
        WorkspaceLocation(location)
        );
}

OperationError gatewayFailure(
    const char* message
    )
{
    return OperationError{
        .code = ErrorCode::Technical,
        .message = message,
        .recoverable = false
    };
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
        Q_UNUSED(request)
        ++saveCalls;
        return Result<void>::success();
    }

    [[nodiscard]] Result<WorkspaceLocation> saveWorkspaceAs(
        const SaveWorkspaceAsRequest& request
        ) override
    {
        Q_UNUSED(request)
        ++saveAsCalls;
        return Result<WorkspaceLocation>::failure(
            gatewayFailure("Unexpected save-as call.")
            );
    }

    [[nodiscard]] Result<WorkspaceLocation> exportWorkspace(
        const ExportWorkspaceRequest& request
        ) override
    {
        Q_UNUSED(request)
        ++exportCalls;
        return Result<WorkspaceLocation>::failure(
            gatewayFailure("Unexpected export call.")
            );
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
    std::optional<Result<WorkspaceSession>> createResponse;
    std::optional<Result<WorkspaceSession>> openResponse;
    std::optional<Result<void>> closeResponse;
};

}

class NextApplicationWorkspaceCoordinatorTests final : public QObject
{
    Q_OBJECT

private slots:
    void createSuccessOpensWorkspaceAndClearsSelection();
    void openSuccessReplacesCleanWorkspaceAndClearsSelection();
    void dirtyCreateIsRejectedBeforeGatewayAndPreservesState();
    void dirtyOpenIsRejectedBeforeGatewayAndPreservesState();
    void gatewayCreateFailurePreservesStateAndSelection();
    void gatewayOpenFailurePreservesStateAndSelection();
    void invalidCreatedSessionUsesStateValidationAndPreservesState();
    void invalidOpenedSessionUsesStateValidationAndPreservesState();
    void closeSuccessUsesCurrentSessionAndClearsSelection();
    void dirtyCloseIsRejectedBeforeGatewayAndPreservesState();
    void gatewayCloseFailurePreservesStateAndSelection();
    void closedCloseReturnsNotFoundWithoutGateway();
};

void NextApplicationWorkspaceCoordinatorTests::createSuccessOpensWorkspaceAndClearsSelection()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceSession expected = testSession(
        "workspace-created",
        "C:/workspaces/created.tps"
        );
    gateway.createResponse = Result<WorkspaceSession>::success(expected);
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    SelectionState selectionState;
    selectionState.setSelection(*TeacherId::fromString("teacher-1"));
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.createWorkspace(
        CreateWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/created.tps")
        }
        );

    QVERIFY(result);
    QVERIFY(result.value() == expected);
    QCOMPARE(gateway.createCalls, 1);
    QVERIFY(workspaceState.snapshot().session().has_value());
    QVERIFY(*workspaceState.snapshot().session() == expected);
    QCOMPARE(
        workspaceState.snapshot().unsavedState(),
        WorkspaceUnsavedState::Clean
        );
    QCOMPARE(selectionState.snapshot().kind(), SelectionKind::None);
}

void NextApplicationWorkspaceCoordinatorTests::openSuccessReplacesCleanWorkspaceAndClearsSelection()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    const WorkspaceSession replacement = testSession(
        "workspace-opened",
        "C:/workspaces/opened.tps"
        );
    gateway.openResponse = Result<WorkspaceSession>::success(replacement);
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    QVERIFY(workspaceState.open(current));
    SelectionState selectionState;
    selectionState.setSelection(*ClassId::fromString("class-1"));
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.openWorkspace(
        OpenWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/opened.tps")
        }
        );

    QVERIFY(result);
    QVERIFY(result.value() == replacement);
    QCOMPARE(gateway.openCalls, 1);
    QVERIFY(workspaceState.snapshot().session().has_value());
    QVERIFY(*workspaceState.snapshot().session() == replacement);
    QCOMPARE(selectionState.snapshot().kind(), SelectionKind::None);
}

void NextApplicationWorkspaceCoordinatorTests::dirtyCreateIsRejectedBeforeGatewayAndPreservesState()
{
    FakeWorkspaceGateway gateway;
    gateway.createResponse = Result<WorkspaceSession>::success(
        testSession("workspace-created", "C:/workspaces/created.tps")
        );
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    QVERIFY(workspaceState.open(current));
    QVERIFY(workspaceState.markDirty());
    const WorkspaceStateSnapshot before = workspaceState.snapshot();
    SelectionState selectionState;
    selectionState.setSelection(*CampusId::fromString("campus-1"));
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.createWorkspace(
        CreateWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/created.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QVERIFY(result.error().recoverable);
    QCOMPARE(gateway.createCalls, 0);
    QVERIFY(workspaceState.snapshot() == before);
    QVERIFY(selectionState.snapshot() == selectionBefore);
}

void NextApplicationWorkspaceCoordinatorTests::dirtyOpenIsRejectedBeforeGatewayAndPreservesState()
{
    FakeWorkspaceGateway gateway;
    gateway.openResponse = Result<WorkspaceSession>::success(
        testSession("workspace-opened", "C:/workspaces/opened.tps")
        );
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    QVERIFY(workspaceState.open(current));
    QVERIFY(workspaceState.markDirty());
    const WorkspaceStateSnapshot before = workspaceState.snapshot();
    SelectionState selectionState;
    selectionState.setSelection(*CalendarEventId::fromString("event-1"));
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.openWorkspace(
        OpenWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/opened.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QVERIFY(result.error().recoverable);
    QCOMPARE(gateway.openCalls, 0);
    QVERIFY(workspaceState.snapshot() == before);
    QVERIFY(selectionState.snapshot() == selectionBefore);
}

void NextApplicationWorkspaceCoordinatorTests::gatewayCreateFailurePreservesStateAndSelection()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError = gatewayFailure(
        "Creating the workspace failed."
        );
    gateway.createResponse = Result<WorkspaceSession>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    QVERIFY(workspaceState.open(current));
    SelectionState selectionState;
    selectionState.setSelection(*TeacherId::fromString("teacher-1"));
    const WorkspaceStateSnapshot before = workspaceState.snapshot();
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.createWorkspace(
        CreateWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/new.tps")
        }
        );

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(gateway.createCalls, 1);
    QVERIFY(workspaceState.snapshot() == before);
    QVERIFY(selectionState.snapshot() == selectionBefore);
}

void NextApplicationWorkspaceCoordinatorTests::gatewayOpenFailurePreservesStateAndSelection()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError = gatewayFailure(
        "Opening the workspace failed."
        );
    gateway.openResponse = Result<WorkspaceSession>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    QVERIFY(workspaceState.open(current));
    SelectionState selectionState;
    selectionState.setSelection(*ClassId::fromString("class-1"));
    const WorkspaceStateSnapshot before = workspaceState.snapshot();
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.openWorkspace(
        OpenWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/missing.tps")
        }
        );

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(gateway.openCalls, 1);
    QVERIFY(workspaceState.snapshot() == before);
    QVERIFY(selectionState.snapshot() == selectionBefore);
}

void NextApplicationWorkspaceCoordinatorTests::invalidCreatedSessionUsesStateValidationAndPreservesState()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceSession invalid = testSession(
        "workspace-invalid",
        " \t\r\n"
        );
    gateway.createResponse = Result<WorkspaceSession>::success(invalid);
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    QVERIFY(workspaceState.open(current));
    SelectionState selectionState;
    selectionState.setSelection(*CampusId::fromString("campus-1"));
    const WorkspaceStateSnapshot before = workspaceState.snapshot();
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.createWorkspace(
        CreateWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/new.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        result.error().message
        == "Workspace session must contain an id and location."
        );
    QCOMPARE(gateway.createCalls, 1);
    QVERIFY(workspaceState.snapshot() == before);
    QVERIFY(selectionState.snapshot() == selectionBefore);
}

void NextApplicationWorkspaceCoordinatorTests::invalidOpenedSessionUsesStateValidationAndPreservesState()
{
    FakeWorkspaceGateway gateway;
    const WorkspaceSession invalid = testSession(
        "workspace-invalid",
        " \t\r\n"
        );
    gateway.openResponse = Result<WorkspaceSession>::success(invalid);
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    QVERIFY(workspaceState.open(current));
    SelectionState selectionState;
    selectionState.setSelection(*CampusId::fromString("campus-1"));
    const WorkspaceStateSnapshot before = workspaceState.snapshot();
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.openWorkspace(
        OpenWorkspaceRequest{
            WorkspaceLocation("C:/workspaces/opened.tps")
        }
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        result.error().message
        == "Workspace session must contain an id and location."
        );
    QCOMPARE(gateway.openCalls, 1);
    QVERIFY(workspaceState.snapshot() == before);
    QVERIFY(selectionState.snapshot() == selectionBefore);
}

void NextApplicationWorkspaceCoordinatorTests::closeSuccessUsesCurrentSessionAndClearsSelection()
{
    FakeWorkspaceGateway gateway;
    gateway.closeResponse = Result<void>::success();
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    QVERIFY(workspaceState.open(current));
    SelectionState selectionState;
    selectionState.setSelection(*CalendarEventId::fromString("event-1"));
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.closeWorkspace();

    QVERIFY(result);
    QCOMPARE(gateway.closeCalls, 1);
    QVERIFY(gateway.lastClosedSession.has_value());
    QVERIFY(*gateway.lastClosedSession == current);
    QCOMPARE(workspaceState.snapshot().lifecycle(), WorkspaceLifecycleState::Closed);
    QCOMPARE(
        workspaceState.snapshot().unsavedState(),
        WorkspaceUnsavedState::Clean
        );
    QCOMPARE(selectionState.snapshot().kind(), SelectionKind::None);
}

void NextApplicationWorkspaceCoordinatorTests::dirtyCloseIsRejectedBeforeGatewayAndPreservesState()
{
    FakeWorkspaceGateway gateway;
    gateway.closeResponse = Result<void>::success();
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    QVERIFY(workspaceState.open(current));
    QVERIFY(workspaceState.markDirty());
    const WorkspaceStateSnapshot before = workspaceState.snapshot();
    SelectionState selectionState;
    selectionState.setSelection(*TeacherId::fromString("teacher-1"));
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.closeWorkspace();

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::Conflict);
    QVERIFY(result.error().recoverable);
    QCOMPARE(gateway.closeCalls, 0);
    QVERIFY(workspaceState.snapshot() == before);
    QVERIFY(selectionState.snapshot() == selectionBefore);
}

void NextApplicationWorkspaceCoordinatorTests::gatewayCloseFailurePreservesStateAndSelection()
{
    FakeWorkspaceGateway gateway;
    const OperationError expectedError = gatewayFailure(
        "Closing the workspace failed."
        );
    gateway.closeResponse = Result<void>::failure(expectedError);
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    const WorkspaceSession current = testSession(
        "workspace-current",
        "C:/workspaces/current.tps"
        );
    QVERIFY(workspaceState.open(current));
    SelectionState selectionState;
    selectionState.setSelection(*ClassId::fromString("class-1"));
    const WorkspaceStateSnapshot before = workspaceState.snapshot();
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.closeWorkspace();

    QVERIFY(!result);
    QVERIFY(result.error() == expectedError);
    QCOMPARE(gateway.closeCalls, 1);
    QVERIFY(workspaceState.snapshot() == before);
    QVERIFY(selectionState.snapshot() == selectionBefore);
}

void NextApplicationWorkspaceCoordinatorTests::closedCloseReturnsNotFoundWithoutGateway()
{
    FakeWorkspaceGateway gateway;
    gateway.closeResponse = Result<void>::success();
    const WorkspaceUseCase useCase(gateway);
    WorkspaceState workspaceState;
    SelectionState selectionState;
    selectionState.setSelection(*TeacherId::fromString("teacher-1"));
    const SelectionStateSnapshot selectionBefore = selectionState.snapshot();
    WorkspaceCoordinator coordinator(useCase, workspaceState, selectionState);

    const auto result = coordinator.closeWorkspace();

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::NotFound);
    QCOMPARE(gateway.closeCalls, 0);
    QVERIFY(selectionState.snapshot() == selectionBefore);
    QCOMPARE(workspaceState.snapshot().lifecycle(), WorkspaceLifecycleState::Closed);
}

QTEST_APPLESS_MAIN(NextApplicationWorkspaceCoordinatorTests)

#include "next_application_workspace_coordinator_tests.moc"

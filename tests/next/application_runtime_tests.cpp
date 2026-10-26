#include "next/application/application_runtime.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

WorkspaceSession makeSession(
    const std::string& id,
    const std::string& location
    )
{
    return WorkspaceSession(
        Domain::WorkspaceId::fromString(id).value(),
        WorkspaceLocation(location)
        );
}

class RecordingWorkspaceGateway final : public WorkspaceGateway
{
public:
    Domain::Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        ) override
    {
        createdLocations.push_back(request.location.value());
        return Domain::Result<WorkspaceSession>::success(
            makeSession("created-id", request.location.value())
            );
    }

    Domain::Result<WorkspaceSession> openWorkspace(
        const OpenWorkspaceRequest& request
        ) override
    {
        openedLocations.push_back(request.location.value());
        if (openError.has_value())
        {
            return Domain::Result<WorkspaceSession>::failure(*openError);
        }
        return Domain::Result<WorkspaceSession>::success(
            makeSession("opened-id", request.location.value())
            );
    }

    Domain::Result<void> closeWorkspace(
        const CloseWorkspaceRequest& request
        ) override
    {
        closedSessions.push_back(request.session);
        if (closeError.has_value())
        {
            return Domain::Result<void>::failure(*closeError);
        }
        return Domain::Result<void>::success();
    }

    Domain::Result<void> saveWorkspace(
        const SaveWorkspaceRequest& request
        ) override
    {
        savedSessions.push_back(request.session);
        return Domain::Result<void>::success();
    }

    Domain::Result<WorkspaceLocation> saveWorkspaceAs(
        const SaveWorkspaceAsRequest& request
        ) override
    {
        savedAsSessions.push_back(request.session);
        saveAsDestinations.push_back(request.destination.value());
        return Domain::Result<WorkspaceLocation>::success(request.destination);
    }

    Domain::Result<WorkspaceLocation> exportWorkspace(
        const ExportWorkspaceRequest& request
        ) override
    {
        exportedSessions.push_back(request.session);
        exportDestinations.push_back(request.destination.value());
        return Domain::Result<WorkspaceLocation>::success(request.destination);
    }

    std::vector<std::string> createdLocations;
    std::vector<std::string> openedLocations;
    std::vector<WorkspaceSession> closedSessions;
    std::vector<WorkspaceSession> savedSessions;
    std::vector<WorkspaceSession> savedAsSessions;
    std::vector<std::string> saveAsDestinations;
    std::vector<WorkspaceSession> exportedSessions;
    std::vector<std::string> exportDestinations;
    std::optional<Domain::OperationError> openError;
    std::optional<Domain::OperationError> closeError;
};

bool expect(
    const bool condition
    )
{
    return condition;
}

bool runtimeRetainsStartupDocumentCatalogProjection()
{
    RecordingWorkspaceGateway gateway;
    ApplicationRuntime runtime(gateway);
    if (!expect(!runtime.startupDocumentCatalogProjection().has_value()))
    {
        return false;
    }

    const auto folderId = Domain::DocumentFolderId::fromString("folder-guides");
    if (!folderId.has_value())
    {
        return false;
    }

    DocumentCatalogProjectionInput input;
    input.folders.push_back(DocumentFolderMetadata{
        .id = *folderId,
        .path = DocumentCatalogPath("Guides"),
        .key = DocumentFolderKey("guides"),
        .displayName = "Guides",
        .order = DocumentFolderOrder(10),
        .parentPath = DocumentCatalogPath("")
    });
    auto projection = DocumentCatalogProjection::create(std::move(input));
    if (!expect(projection.hasValue()))
    {
        return false;
    }

    runtime.retainStartupDocumentCatalogProjection(std::move(projection.value()));
    const auto& retained = runtime.startupDocumentCatalogProjection();
    return expect(retained.has_value())
        && expect(retained->folders().size() == 1)
        && expect(retained->documents().empty())
        && expect(retained->folders().front().id == *folderId)
        && expect(retained->folders().front().path.value() == "Guides")
        && expect(retained->folders().front().displayName == "Guides");
}

bool emptyRuntimeCanCreateAndOpenLater(RecordingWorkspaceGateway& gateway)
{
    ApplicationRuntime runtime(gateway);
    if (!expect(runtime.workspaceState().snapshot().lifecycle()
                    == WorkspaceLifecycleState::Closed)
        || !expect(!runtime.workspaceState().snapshot().session().has_value())
        || !expect(!runtime.selectionState().snapshot().hasSelection()))
    {
        return false;
    }

    const auto created = runtime.workspaceCoordinator().createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation("new-profile.tps")}
        );
    if (!expect(created.hasValue())
        || !expect(created.value().location().value() == "new-profile.tps")
        || !expect(runtime.workspaceState().snapshot().session() == created.value()))
    {
        return false;
    }

    if (!runtime.workspaceState().markSaved())
    {
        return false;
    }
    const auto closed = runtime.workspaceCoordinator().closeWorkspace();
    if (!expect(closed.hasValue())
        || !expect(runtime.workspaceState().snapshot().lifecycle()
                    == WorkspaceLifecycleState::Closed))
    {
        return false;
    }

    const auto opened = runtime.workspaceCoordinator().openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("existing-profile.tps")}
        );
    return expect(opened.hasValue())
        && expect(opened.value().location().value() == "existing-profile.tps")
        && expect(runtime.workspaceState().snapshot().session() == opened.value());
}

bool coordinatorDelegatesOperationsAndHonorsStateGuards(
    RecordingWorkspaceGateway& gateway
    )
{
    ApplicationRuntime runtime(gateway);
    const auto selectionId = Domain::ClassId::fromString("class-before-create");
    if (!selectionId.has_value())
    {
        return false;
    }
    runtime.selectionState().setSelection(*selectionId);

    const auto created = runtime.workspaceCoordinator().createWorkspace(
        CreateWorkspaceRequest{WorkspaceLocation("coordinator-profile.tps")}
        );
    if (!expect(created.hasValue())
        || !expect(!runtime.selectionState().snapshot().hasSelection())
        || !expect(gateway.createdLocations.size() == 1)
        || !expect(gateway.createdLocations.front() == "coordinator-profile.tps"))
    {
        return false;
    }

    if (!runtime.workspaceState().markDirty())
    {
        return false;
    }
    const auto blockedOpen = runtime.workspaceCoordinator().openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("must-not-open.tps")}
        );
    const auto blockedClose = runtime.workspaceCoordinator().closeWorkspace();
    if (!expect(!blockedOpen.hasValue())
        || !expect(blockedOpen.error().code == Domain::ErrorCode::Conflict)
        || !expect(!blockedClose.hasValue())
        || !expect(blockedClose.error().code == Domain::ErrorCode::Conflict)
        || !expect(gateway.openedLocations.empty())
        || !expect(gateway.closedSessions.empty()))
    {
        return false;
    }

    if (!runtime.workspaceState().markSaved())
    {
        return false;
    }
    const auto saved = runtime.workspaceCoordinator().saveWorkspace();
    const auto savedAs = runtime.workspaceCoordinator().saveWorkspaceAs(
        WorkspaceLocation("renamed-profile.tps")
        );
    const auto exported = runtime.workspaceCoordinator().exportWorkspace(
        WorkspaceLocation("profile-export.tps")
        );
    if (!expect(saved.hasValue())
        || !expect(savedAs.hasValue())
        || !expect(savedAs.value().value() == "renamed-profile.tps")
        || !expect(exported.hasValue())
        || !expect(exported.value().value() == "profile-export.tps")
        || !expect(gateway.savedSessions.size() == 1)
        || !expect(gateway.savedSessions.front() == created.value())
        || !expect(gateway.savedAsSessions.size() == 1)
        || !expect(gateway.savedAsSessions.front() == created.value())
        || !expect(gateway.saveAsDestinations.front() == "renamed-profile.tps")
        || !expect(gateway.exportedSessions.size() == 1)
        || !expect(gateway.exportedSessions.front().location().value()
                    == "renamed-profile.tps")
        || !expect(gateway.exportDestinations.front() == "profile-export.tps"))
    {
        return false;
    }

    const auto closed = runtime.workspaceCoordinator().closeWorkspace();
    return expect(closed.hasValue())
        && expect(gateway.closedSessions.size() == 1)
        && expect(gateway.closedSessions.front().workspaceId()
                    == created.value().workspaceId())
        && expect(gateway.closedSessions.front().location().value()
                    == "renamed-profile.tps")
        && expect(runtime.workspaceState().snapshot().lifecycle()
                    == WorkspaceLifecycleState::Closed);
}

bool failedAdapterCallsDoNotCommitRuntimeState()
{
    RecordingWorkspaceGateway gateway;
    gateway.openError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected open failure",
        .recoverable = true
    };
    ApplicationRuntime runtime(gateway);

    const auto failedOpen = runtime.workspaceCoordinator().openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("unreadable-profile.tps")}
        );
    if (!expect(!failedOpen.hasValue())
        || !expect(failedOpen.error().code == Domain::ErrorCode::Technical)
        || !expect(failedOpen.error().message == "injected open failure")
        || !expect(runtime.workspaceState().snapshot().lifecycle()
                    == WorkspaceLifecycleState::Closed))
    {
        return false;
    }

    gateway.openError.reset();
    const auto opened = runtime.workspaceCoordinator().openWorkspace(
        OpenWorkspaceRequest{WorkspaceLocation("readable-profile.tps")}
        );
    if (!opened.hasValue())
    {
        return false;
    }

    gateway.closeError = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected close failure",
        .recoverable = true
    };
    const auto failedClose = runtime.workspaceCoordinator().closeWorkspace();
    return expect(!failedClose.hasValue())
        && expect(failedClose.error().message == "injected close failure")
        && expect(runtime.workspaceState().snapshot().session() == opened.value());
}

} // namespace

static_assert(!std::is_copy_constructible_v<ApplicationRuntime>);
static_assert(!std::is_move_constructible_v<ApplicationRuntime>);

int main()
{
    RecordingWorkspaceGateway emptyStartGateway;
    if (!emptyRuntimeCanCreateAndOpenLater(emptyStartGateway))
    {
        return EXIT_FAILURE;
    }

    RecordingWorkspaceGateway coordinatorGateway;
    if (!coordinatorDelegatesOperationsAndHonorsStateGuards(coordinatorGateway))
    {
        return EXIT_FAILURE;
    }

    if (!failedAdapterCallsDoNotCommitRuntimeState())
    {
        return EXIT_FAILURE;
    }

    if (!runtimeRetainsStartupDocumentCatalogProjection())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

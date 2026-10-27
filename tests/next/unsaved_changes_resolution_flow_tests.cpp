#include "next/application/application_runtime.h"
#include "next/application/unsaved_changes_resolution_flow.h"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::OperationError testError(std::string message)
{
    return {
        .code = Domain::ErrorCode::Technical,
        .message = std::move(message),
        .recoverable = true
    };
}

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

class WorkspaceGatewayProbe final : public WorkspaceGateway
{
public:
    Domain::Result<WorkspaceSession> createWorkspace(
        const CreateWorkspaceRequest& request
        ) override
    {
        return Domain::Result<WorkspaceSession>::success(
            makeSession("created", request.location.value())
            );
    }

    Domain::Result<WorkspaceSession> openWorkspace(
        const OpenWorkspaceRequest& request
        ) override
    {
        return Domain::Result<WorkspaceSession>::success(
            makeSession("opened", request.location.value())
            );
    }

    Domain::Result<void> closeWorkspace(
        const CloseWorkspaceRequest& request
        ) override
    {
        static_cast<void>(request);
        ++closeCount;
        return Domain::Result<void>::success();
    }

    Domain::Result<void> saveWorkspace(
        const SaveWorkspaceRequest& request
        ) override
    {
        static_cast<void>(request);
        ++saveCount;
        return saveFailure
            ? Domain::Result<void>::failure(testError("injected save failure"))
            : Domain::Result<void>::success();
    }

    Domain::Result<void> discardWorkspaceChanges(
        const DiscardWorkspaceChangesRequest& request
        ) override
    {
        static_cast<void>(request);
        ++discardCount;
        return discardFailure
            ? Domain::Result<void>::failure(testError("injected discard failure"))
            : Domain::Result<void>::success();
    }

    Domain::Result<WorkspaceLocation> saveWorkspaceAs(
        const SaveWorkspaceAsRequest& request
        ) override
    {
        return Domain::Result<WorkspaceLocation>::success(request.destination);
    }

    Domain::Result<WorkspaceLocation> exportWorkspace(
        const ExportWorkspaceRequest& request
        ) override
    {
        return Domain::Result<WorkspaceLocation>::success(request.destination);
    }

    int closeCount = 0;
    int saveCount = 0;
    int discardCount = 0;
    bool saveFailure = false;
    bool discardFailure = false;
};

class ActivePageChangesProbe final : public ActivePageChangesPort
{
public:
    [[nodiscard]] bool hasUnsavedChanges() const noexcept override
    {
        return dirty;
    }

    [[nodiscard]] Domain::Result<void> saveChanges() override
    {
        ++saveCount;
        if (saveFailure)
        {
            return Domain::Result<void>::failure(testError("injected page save failure"));
        }
        dirty = false;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> discardChanges() override
    {
        ++discardCount;
        if (discardFailure)
        {
            return Domain::Result<void>::failure(testError("injected page discard failure"));
        }
        dirty = false;
        return Domain::Result<void>::success();
    }

    bool dirty = false;
    bool saveFailure = false;
    bool discardFailure = false;
    int saveCount = 0;
    int discardCount = 0;
};

class UserPromptProbe final : public UserPromptPort
{
public:
    Domain::Result<void> showMessage(const MessagePromptRequest&) override
    {
        return Domain::Result<void>::success();
    }

    Domain::Result<ConfirmationResponse> confirm(
        const ConfirmationPromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<ConfirmationResponse>::success(
            ConfirmationResponse::Accepted
            );
    }

    Domain::Result<UnsavedChangesAction> confirmUnsavedChanges(
        const UnsavedChangesPromptRequest& request
        ) override
    {
        ++promptCount;
        lastRequest = request;
        if (promptFailure)
        {
            return Domain::Result<UnsavedChangesAction>::failure(
                testError("injected prompt failure")
                );
        }
        return Domain::Result<UnsavedChangesAction>::success(action);
    }

    Domain::Result<std::optional<PromptActionId>> chooseAction(
        const NamedActionPromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<std::optional<PromptActionId>>::success(std::nullopt);
    }

    UnsavedChangesAction action = UnsavedChangesAction::Cancel;
    bool promptFailure = false;
    int promptCount = 0;
    UnsavedChangesPromptRequest lastRequest;
};

struct FlowFixture final
{
    WorkspaceGatewayProbe gateway;
    ApplicationRuntime runtime{gateway};
    ActivePageChangesProbe page;
    UserPromptProbe prompt;

    FlowFixture()
    {
        static_cast<void>(runtime.workspaceCoordinator().openWorkspace(
            OpenWorkspaceRequest{WorkspaceLocation("existing.tps")}
            ));
    }

    [[nodiscard]] UnsavedChangesResolution resolve()
    {
        UnsavedChangesResolutionFlow flow(
            page,
            runtime.workspaceCoordinator(),
            prompt
            );
        return flow.resolve(UnsavedChangesPromptRequest{
            .title = "Unsaved Changes",
            .message = "The page has a pending draft.",
            .question = "Save before leaving?",
            .saveText = "Save",
            .discardText = "Discard",
            .cancelText = "Cancel"
        });
    }
};

bool cleanStateDoesNotPrompt()
{
    FlowFixture fixture;
    const auto result = fixture.resolve();
    return result.status == UnsavedChangesResolutionStatus::Ready
        && fixture.prompt.promptCount == 0
        && fixture.gateway.saveCount == 0
        && fixture.gateway.discardCount == 0;
}

bool pageSaveThenWorkspaceCommitResolvesDirtyState()
{
    FlowFixture fixture;
    fixture.page.dirty = true;
    if (!fixture.runtime.workspaceState().markDirty())
    {
        return false;
    }
    fixture.prompt.action = UnsavedChangesAction::Save;

    const auto result = fixture.resolve();
    return result.status == UnsavedChangesResolutionStatus::Ready
        && fixture.page.saveCount == 1
        && !fixture.page.dirty
        && fixture.gateway.saveCount == 1
        && !fixture.runtime.workspaceCoordinator().hasUnsavedChanges()
        && fixture.runtime.workspaceState().snapshot().session().has_value();
}

bool discardRevertsPageDraftBeforeWorkspaceRollback()
{
    FlowFixture fixture;
    fixture.page.dirty = true;
    if (!fixture.runtime.workspaceState().markDirty())
    {
        return false;
    }
    fixture.prompt.action = UnsavedChangesAction::Discard;

    const auto result = fixture.resolve();
    return result.status == UnsavedChangesResolutionStatus::Ready
        && fixture.page.discardCount == 1
        && !fixture.page.dirty
        && fixture.gateway.discardCount == 1
        && !fixture.runtime.workspaceCoordinator().hasUnsavedChanges()
        && fixture.runtime.workspaceState().snapshot().session().has_value();
}

bool cancelAndPromptFailurePreserveDirtyDraft()
{
    FlowFixture fixture;
    fixture.page.dirty = true;
    fixture.prompt.action = UnsavedChangesAction::Cancel;
    const auto canceled = fixture.resolve();
    if (canceled.status != UnsavedChangesResolutionStatus::Canceled
        || !fixture.page.dirty
        || fixture.page.saveCount != 0
        || fixture.page.discardCount != 0)
    {
        return false;
    }

    fixture.prompt.promptFailure = true;
    const auto failed = fixture.resolve();
    return failed.status == UnsavedChangesResolutionStatus::Failed
        && !failed.message.empty()
        && fixture.page.dirty
        && fixture.page.saveCount == 0
        && fixture.page.discardCount == 0;
}

bool saveAndDiscardFailuresDoNotClearDirtyState()
{
    FlowFixture fixture;
    fixture.page.dirty = true;
    if (!fixture.runtime.workspaceState().markDirty())
    {
        return false;
    }
    fixture.prompt.action = UnsavedChangesAction::Save;
    fixture.page.saveFailure = true;
    const auto pageSaveFailure = fixture.resolve();
    if (pageSaveFailure.status != UnsavedChangesResolutionStatus::Failed
        || !fixture.page.dirty
        || !fixture.runtime.workspaceCoordinator().hasUnsavedChanges()
        || fixture.gateway.saveCount != 0)
    {
        return false;
    }

    fixture.page.saveFailure = false;
    fixture.gateway.saveFailure = true;
    const auto workspaceSaveFailure = fixture.resolve();
    if (workspaceSaveFailure.status != UnsavedChangesResolutionStatus::Failed
        || fixture.page.dirty
        || !fixture.runtime.workspaceCoordinator().hasUnsavedChanges())
    {
        return false;
    }

    fixture.page.dirty = true;
    fixture.page.discardFailure = true;
    fixture.prompt.action = UnsavedChangesAction::Discard;
    const auto pageDiscardFailure = fixture.resolve();
    if (pageDiscardFailure.status != UnsavedChangesResolutionStatus::Failed
        || !fixture.page.dirty
        || !fixture.runtime.workspaceCoordinator().hasUnsavedChanges()
        || fixture.gateway.discardCount != 0)
    {
        return false;
    }

    fixture.page.discardFailure = false;
    fixture.gateway.discardFailure = true;
    const auto workspaceDiscardFailure = fixture.resolve();
    return workspaceDiscardFailure.status == UnsavedChangesResolutionStatus::Failed
        && !fixture.page.dirty
        && fixture.runtime.workspaceCoordinator().hasUnsavedChanges()
        && fixture.runtime.workspaceState().snapshot().session().has_value();
}

bool discardCanRollbackWorkspaceOnlyWhenTheStoreAcceptsIt()
{
    FlowFixture fixture;
    if (!fixture.runtime.workspaceState().markDirty())
    {
        return false;
    }
    fixture.prompt.action = UnsavedChangesAction::Discard;
    const auto result = fixture.resolve();
    return result.status == UnsavedChangesResolutionStatus::Ready
        && fixture.gateway.discardCount == 1
        && !fixture.runtime.workspaceCoordinator().hasUnsavedChanges();
}

} // namespace

int main()
{
    if (!cleanStateDoesNotPrompt()
        || !pageSaveThenWorkspaceCommitResolvesDirtyState()
        || !discardRevertsPageDraftBeforeWorkspaceRollback()
        || !cancelAndPromptFailurePreserveDirtyDraft()
        || !saveAndDiscardFailuresDoNotClearDirtyState()
        || !discardCanRollbackWorkspaceOnlyWhenTheStoreAcceptsIt())
    {
        std::cerr << "Next unsaved-changes resolution tests failed.\n";
        return 1;
    }
    std::cout << "Next unsaved-changes resolution tests passed.\n";
    return 0;
}

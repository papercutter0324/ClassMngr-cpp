#pragma once

#include "next/application/page_changes_port.h"
#include "next/application/user_prompt_port.h"
#include "next/application/workspace_coordinator.h"

#include <string>

namespace ClassMngr::Next::Application
{

enum class UnsavedChangesResolutionStatus
{
    Ready,
    Canceled,
    Failed
};

struct UnsavedChangesResolution final
{
    UnsavedChangesResolutionStatus status = UnsavedChangesResolutionStatus::Failed;
    std::string message;
};

// Resolves an active page draft before the caller navigates, changes profiles,
// closes a profile, or shuts down. Page drafts stay distinct from the
// coordinator's database commit state.
class UnsavedChangesResolutionFlow final
{
public:
    UnsavedChangesResolutionFlow(
        ActivePageChangesPort& activePage,
        WorkspaceCoordinator& workspaceCoordinator,
        UserPromptPort& userPrompt
        ) noexcept
        : m_activePage(activePage),
          m_workspaceCoordinator(workspaceCoordinator),
          m_userPrompt(userPrompt)
    {
    }

    [[nodiscard]] UnsavedChangesResolution resolve(
        const UnsavedChangesPromptRequest& request
        )
    {
        const bool pageDirty = m_activePage.hasUnsavedChanges();
        const bool workspaceDirty = m_workspaceCoordinator.hasUnsavedChanges();
        if (!pageDirty && !workspaceDirty)
        {
            return {UnsavedChangesResolutionStatus::Ready, {}};
        }

        const Domain::Result<UnsavedChangesAction> response =
            m_userPrompt.confirmUnsavedChanges(request);
        if (!response)
        {
            return {
                UnsavedChangesResolutionStatus::Failed,
                "The unsaved-changes prompt failed: " + response.error().message
            };
        }

        switch (response.value())
        {
        case UnsavedChangesAction::Cancel:
            return {UnsavedChangesResolutionStatus::Canceled, {}};
        case UnsavedChangesAction::Save:
        {
            if (pageDirty)
            {
                const Domain::Result<void> pageSave = m_activePage.saveChanges();
                if (!pageSave)
                {
                    return {
                        UnsavedChangesResolutionStatus::Failed,
                        "The current page could not save its changes: "
                            + pageSave.error().message
                    };
                }
                if (m_activePage.hasUnsavedChanges())
                {
                    return {
                        UnsavedChangesResolutionStatus::Failed,
                        "The current page still has unsaved changes after saving."
                    };
                }
            }

            if (m_workspaceCoordinator.hasUnsavedChanges())
            {
                const Domain::Result<void> workspaceSave =
                    m_workspaceCoordinator.saveWorkspace();
                if (!workspaceSave)
                {
                    return {
                        UnsavedChangesResolutionStatus::Failed,
                        "The workspace could not save its changes: "
                            + workspaceSave.error().message
                    };
                }
            }

            if (m_workspaceCoordinator.hasUnsavedChanges())
            {
                return {
                    UnsavedChangesResolutionStatus::Failed,
                    "The workspace still has unsaved changes after saving."
                };
            }
            return {UnsavedChangesResolutionStatus::Ready, {}};
        }
        case UnsavedChangesAction::Discard:
        {
            if (pageDirty)
            {
                const Domain::Result<void> pageDiscard =
                    m_activePage.discardChanges();
                if (!pageDiscard)
                {
                    return {
                        UnsavedChangesResolutionStatus::Failed,
                        "The current page could not discard its changes: "
                            + pageDiscard.error().message
                    };
                }
                if (m_activePage.hasUnsavedChanges())
                {
                    return {
                        UnsavedChangesResolutionStatus::Failed,
                        "The current page still has unsaved changes after discarding."
                    };
                }
            }

            if (m_workspaceCoordinator.hasUnsavedChanges())
            {
                const Domain::Result<void> discarded =
                    m_workspaceCoordinator.discardWorkspaceChanges();
                if (!discarded)
                {
                    return {
                        UnsavedChangesResolutionStatus::Failed,
                        "The workspace could not clear its discarded changes: "
                            + discarded.error().message
                    };
                }
            }
            return {UnsavedChangesResolutionStatus::Ready, {}};
        }
        }

        return {
            UnsavedChangesResolutionStatus::Failed,
            "The unsaved-changes prompt returned an unknown action."
        };
    }

private:
    ActivePageChangesPort& m_activePage;
    WorkspaceCoordinator& m_workspaceCoordinator;
    UserPromptPort& m_userPrompt;
};

} // namespace ClassMngr::Next::Application

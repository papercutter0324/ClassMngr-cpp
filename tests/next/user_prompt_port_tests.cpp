#include "next/application/user_prompt_port.h"

#include <iostream>
#include <optional>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class FakeUserPromptPort final : public UserPromptPort
{
public:
    Domain::Result<void> showMessage(
        const MessagePromptRequest& request
        ) override
    {
        ++messageCalls;
        lastMessage = request;
        return messageResult;
    }

    Domain::Result<ConfirmationResponse> confirm(
        const ConfirmationPromptRequest& request
        ) override
    {
        ++confirmationCalls;
        lastConfirmation = request;
        return confirmationResult;
    }

    Domain::Result<UnsavedChangesAction> confirmUnsavedChanges(
        const UnsavedChangesPromptRequest& request
        ) override
    {
        ++unsavedChangesCalls;
        lastUnsavedChanges = request;
        return unsavedChangesResult;
    }

    Domain::Result<std::optional<PromptActionId>> chooseAction(
        const NamedActionPromptRequest& request
        ) override
    {
        ++namedActionCalls;
        lastNamedAction = request;
        return namedActionResult;
    }

    int messageCalls = 0;
    int confirmationCalls = 0;
    int unsavedChangesCalls = 0;
    int namedActionCalls = 0;
    MessagePromptRequest lastMessage;
    ConfirmationPromptRequest lastConfirmation;
    UnsavedChangesPromptRequest lastUnsavedChanges;
    NamedActionPromptRequest lastNamedAction;
    Domain::Result<void> messageResult = Domain::Result<void>::success();
    Domain::Result<ConfirmationResponse> confirmationResult =
        Domain::Result<ConfirmationResponse>::success(
            ConfirmationResponse::Dismissed
            );
    Domain::Result<UnsavedChangesAction> unsavedChangesResult =
        Domain::Result<UnsavedChangesAction>::success(
            UnsavedChangesAction::Cancel
            );
    Domain::Result<std::optional<PromptActionId>> namedActionResult =
        Domain::Result<std::optional<PromptActionId>>::success(std::nullopt);
};

bool actionIdsAreValidatedStableValues()
{
    const auto stableId = PromptActionId::create("teacher.delete.confirm");
    return stableId
        && stableId->value() == "teacher.delete.confirm"
        && !PromptActionId::create("")
        && !PromptActionId::create("Delete")
        && !PromptActionId::create("delete action")
        && !PromptActionId::create(".delete")
        && !PromptActionId::create(std::string(97, 'a'));
}

bool fakePortReceivesSemanticRequestsAndReturnsNormalChoices()
{
    FakeUserPromptPort port;
    const auto deleteAction = PromptActionId::create("delete");
    const auto applyAction = PromptActionId::create("apply");
    if (!deleteAction || !applyAction)
    {
        return false;
    }

    const MessagePromptRequest message{
        .title = "Saved",
        .message = "The report was saved.",
        .details = "One report was written.",
        .severity = PromptSeverity::Information,
        .acknowledgeText = "Close"
    };
    const ConfirmationPromptRequest confirmation{
        .title = "Delete report",
        .message = "Delete this report?",
        .severity = PromptSeverity::Warning,
        .acceptText = "Delete",
        .declineText = "Keep",
        .acceptButtonRole = ConfirmationButtonRole::Destructive,
        .defaultAction = ConfirmationAction::Decline,
        .escapeAction = ConfirmationAction::Decline
    };
    const UnsavedChangesPromptRequest unsaved{
        .title = "Unsaved changes",
        .message = "This page has unsaved changes.",
        .question = "Save before leaving?"
    };
    const NamedActionPromptRequest namedAction{
        .title = "Choose an action",
        .message = "Choose one action.",
        .informativeText = "The action may take time.",
        .severity = PromptSeverity::Warning,
        .actions = {
            PromptAction{
                .id = *applyAction,
                .text = "Apply",
                .role = PromptActionRole::Accept,
                .enabled = false
            },
            PromptAction{
                .id = *deleteAction,
                .text = "Delete",
                .role = PromptActionRole::Destructive
            }
        },
        .defaultActionId = *deleteAction,
        .escapeActionId = *applyAction
    };

    port.confirmationResult = Domain::Result<ConfirmationResponse>::success(
        ConfirmationResponse::Dismissed
        );
    port.namedActionResult =
        Domain::Result<std::optional<PromptActionId>>::success(*deleteAction);

    const auto messageResult = port.showMessage(message);
    const auto confirmationResult = port.confirm(confirmation);
    const auto unsavedResult = port.confirmUnsavedChanges(unsaved);
    const auto namedActionResult = port.chooseAction(namedAction);

    return messageResult.hasValue()
        && confirmationResult.hasValue()
        && confirmationResult.value() == ConfirmationResponse::Dismissed
        && unsavedResult.hasValue()
        && unsavedResult.value() == UnsavedChangesAction::Cancel
        && namedActionResult.hasValue()
        && namedActionResult.value() == *deleteAction
        && port.messageCalls == 1
        && port.confirmationCalls == 1
        && port.unsavedChangesCalls == 1
        && port.namedActionCalls == 1
        && port.lastMessage.details == message.details
        && port.lastConfirmation.acceptButtonRole
            == ConfirmationButtonRole::Destructive
        && port.lastConfirmation.defaultAction == ConfirmationAction::Decline
        && port.lastUnsavedChanges.defaultAction == UnsavedChangesAction::Cancel
        && port.lastNamedAction.actions.size() == 2
        && port.lastNamedAction.defaultActionId == deleteAction;
}

bool technicalErrorsRemainDistinctFromUserDismissal()
{
    FakeUserPromptPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "The presenter could not create a prompt.",
        .recoverable = true
    };
    port.confirmationResult =
        Domain::Result<ConfirmationResponse>::failure(expected);

    const auto failed = port.confirm(ConfirmationPromptRequest{});
    if (failed.hasValue() || failed.error() != expected)
    {
        return false;
    }

    port.confirmationResult = Domain::Result<ConfirmationResponse>::success(
        ConfirmationResponse::Dismissed
        );
    const auto dismissed = port.confirm(ConfirmationPromptRequest{});
    return dismissed.hasValue()
        && dismissed.value() == ConfirmationResponse::Dismissed;
}

} // namespace

int main()
{
    if (!actionIdsAreValidatedStableValues()
        || !fakePortReceivesSemanticRequestsAndReturnsNormalChoices()
        || !technicalErrorsRemainDistinctFromUserDismissal())
    {
        std::cerr << "Next user-prompt port tests failed.\n";
        return 1;
    }
    std::cout << "Next user-prompt port tests passed.\n";
    return 0;
}

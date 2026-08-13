#pragma once

#include "ui/shared/dialogs/user_prompt_service.h"

#include <QQueue>
#include <QVector>

class FakeUserPromptService final : public IUserPromptService
{
public:
    void showMessage(
        const PromptRequest& request
        ) override
    {
        messages.append(request);
    }

    void showMessageAsync(
        const PromptRequest& request
        ) override
    {
        asynchronousMessages.append(request);
    }

    [[nodiscard]] PromptChoice confirm(
        const PromptRequest& request
        ) override
    {
        confirmations.append(request);
        return scriptedChoices.isEmpty()
            ? PromptChoice::Rejected
            : scriptedChoices.dequeue();
    }

    [[nodiscard]] UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest& request
        ) override
    {
        unsavedChangesConfirmations.append(request);
        return scriptedUnsavedChangesChoices.isEmpty()
            ? UnsavedChangesChoice::Cancel
            : scriptedUnsavedChangesChoices.dequeue();
    }

    [[nodiscard]] QString chooseAction(
        const ActionPromptRequest& request
        ) override
    {
        actionPrompts.append(request);
        return scriptedActionIds.isEmpty()
            ? QString()
            : scriptedActionIds.dequeue();
    }

    QVector<PromptRequest> messages;
    QVector<PromptRequest> asynchronousMessages;
    QVector<PromptRequest> confirmations;
    QVector<UnsavedChangesRequest> unsavedChangesConfirmations;
    QVector<ActionPromptRequest> actionPrompts;
    QQueue<PromptChoice> scriptedChoices;
    QQueue<UnsavedChangesChoice> scriptedUnsavedChangesChoices;
    QQueue<QString> scriptedActionIds;
};

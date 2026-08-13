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

    [[nodiscard]] PromptChoice confirm(
        const PromptRequest& request
        ) override
    {
        confirmations.append(request);
        return scriptedChoices.isEmpty()
            ? PromptChoice::Rejected
            : scriptedChoices.dequeue();
    }

    QVector<PromptRequest> messages;
    QVector<PromptRequest> confirmations;
    QQueue<PromptChoice> scriptedChoices;
};

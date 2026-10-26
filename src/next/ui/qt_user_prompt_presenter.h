#pragma once

#include "next/application/user_prompt_port.h"
#include "next/ui/prompt_automation.h"

class QWidget;

namespace ClassMngr::Next::Ui
{

// Synchronous Qt implementation of the Qt-free application port. The parent
// is borrowed for this presenter's lifetime and is never part of a request.
class QtUserPromptPresenter final : public Application::UserPromptPort
{
public:
    explicit QtUserPromptPresenter(QWidget* parent = nullptr) noexcept;

    [[nodiscard]] Domain::Result<void> showMessage(
        const Application::MessagePromptRequest& request
        ) override;

    [[nodiscard]] Domain::Result<Application::ConfirmationResponse> confirm(
        const Application::ConfirmationPromptRequest& request
        ) override;

    [[nodiscard]] Domain::Result<Application::UnsavedChangesAction>
    confirmUnsavedChanges(
        const Application::UnsavedChangesPromptRequest& request
        ) override;

    [[nodiscard]] Domain::Result<
        std::optional<Application::PromptActionId>> chooseAction(
        const Application::NamedActionPromptRequest& request
        ) override;

private:
    QWidget* m_parent = nullptr;
};

class QtPromptAutomationAdapter final : public PromptAutomationDriver
{
public:
    [[nodiscard]] std::optional<PromptSnapshot> activePrompt(
        std::optional<PromptAutomationId> id = std::nullopt
        ) const override;

    bool acknowledge(const PromptAutomationId& id) override;
    bool chooseConfirmation(
        const PromptAutomationId& id,
        Application::ConfirmationAction action
        ) override;
    bool chooseUnsavedChanges(
        const PromptAutomationId& id,
        Application::UnsavedChangesAction action
        ) override;
    bool chooseAction(
        const PromptAutomationId& id,
        const Application::PromptActionId& actionId
        ) override;
    bool activateDefault(const PromptAutomationId& id) override;
    bool reject(const PromptAutomationId& id) override;
    bool dismiss(const PromptAutomationId& id) override;
    bool capture(
        const PromptAutomationId& id,
        const std::string& path
        ) override;
};

} // namespace ClassMngr::Next::Ui

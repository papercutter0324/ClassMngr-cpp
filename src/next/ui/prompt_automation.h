#pragma once

#include "next/application/user_prompt_port.h"

#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Ui
{

enum class PromptKind
{
    Message,
    Confirmation,
    UnsavedChanges,
    NamedAction
};

// Presentation automation identity is intentionally owned by the UI layer;
// it is not part of any application prompt request.
class PromptAutomationId final
{
public:
    [[nodiscard]] static std::optional<PromptAutomationId> create(
        std::string value
        );

    [[nodiscard]] const std::string& value() const noexcept;

    friend bool operator==(
        const PromptAutomationId&,
        const PromptAutomationId&
        ) = default;

private:
    explicit PromptAutomationId(std::string value);

    std::string m_value;
};

struct PromptActionSnapshot final
{
    std::optional<Application::PromptActionId> id;
    std::string text;
    Application::PromptActionRole role = Application::PromptActionRole::Action;
    bool enabled = true;
    bool isDefault = false;
    bool isEscape = false;
};

struct PromptSnapshot final
{
    PromptAutomationId id;
    PromptKind kind = PromptKind::Message;
    std::string title;
    std::string text;
    std::string informativeText;
    std::string details;
    Application::PromptSeverity severity =
        Application::PromptSeverity::Information;
    bool visible = false;
    std::vector<PromptActionSnapshot> actions;
};

// This test/visual automation surface returns only values. All widget access
// stays in the Qt adapter implementation.
class PromptAutomationDriver
{
public:
    virtual ~PromptAutomationDriver() = default;

    [[nodiscard]] virtual std::optional<PromptSnapshot> activePrompt(
        std::optional<PromptAutomationId> id = std::nullopt
        ) const = 0;

    virtual bool acknowledge(const PromptAutomationId& id) = 0;
    virtual bool chooseConfirmation(
        const PromptAutomationId& id,
        Application::ConfirmationAction action
        ) = 0;
    virtual bool chooseUnsavedChanges(
        const PromptAutomationId& id,
        Application::UnsavedChangesAction action
        ) = 0;
    virtual bool chooseAction(
        const PromptAutomationId& id,
        const Application::PromptActionId& actionId
        ) = 0;
    virtual bool activateDefault(const PromptAutomationId& id) = 0;
    // Follows the configured Escape action, or rejects the dialog when none
    // exists. Use dismiss() to close without selecting an action.
    virtual bool reject(const PromptAutomationId& id) = 0;
    virtual bool dismiss(const PromptAutomationId& id) = 0;
    virtual bool capture(
        const PromptAutomationId& id,
        const std::string& path
        ) = 0;
};

} // namespace ClassMngr::Next::Ui

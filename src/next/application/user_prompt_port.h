#pragma once

#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class PromptSeverity
{
    Information,
    Warning,
    Error
};

enum class ConfirmationAction
{
    Accept,
    Decline
};

enum class ConfirmationResponse
{
    Accepted,
    Declined,
    Dismissed
};

// Button appearance is independent of the user's semantic response. A
// destructive Accept button still produces ConfirmationResponse::Accepted.
enum class ConfirmationButtonRole
{
    Accept,
    Destructive
};

enum class UnsavedChangesAction
{
    Save,
    Discard,
    Cancel
};

enum class PromptActionRole
{
    Accept,
    Reject,
    Destructive,
    Action
};

// Stable semantic IDs are lowercase ASCII tokens. They identify an action in
// application data and are deliberately separate from UI prompt automation
// IDs.
class PromptActionId final
{
public:
    [[nodiscard]] static std::optional<PromptActionId> create(
        std::string value
        )
    {
        if (value.empty() || value.size() > 96)
        {
            return std::nullopt;
        }

        const auto isLowerOrDigit = [](const char character)
        {
            return (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9');
        };
        if (!isLowerOrDigit(value.front()))
        {
            return std::nullopt;
        }

        for (const char character : value)
        {
            if (!isLowerOrDigit(character)
                && character != '.'
                && character != '_'
                && character != '-')
            {
                return std::nullopt;
            }
        }

        return PromptActionId(std::move(value));
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const PromptActionId&,
        const PromptActionId&
        ) = default;

private:
    explicit PromptActionId(std::string value)
        : m_value(std::move(value))
    {
    }

    std::string m_value;
};

struct MessagePromptRequest final
{
    std::string title;
    std::string message;
    std::string details;
    PromptSeverity severity = PromptSeverity::Information;
    // The presenter's sole acknowledge action is its default and Escape action.
    std::string acknowledgeText;
};

struct ConfirmationPromptRequest final
{
    std::string title;
    std::string message;
    PromptSeverity severity = PromptSeverity::Information;
    std::string acceptText;
    std::string declineText;
    ConfirmationButtonRole acceptButtonRole = ConfirmationButtonRole::Accept;
    // When omitted, a normal accept button is the default, while a
    // destructive accept button keeps the legacy safe default of Decline.
    std::optional<ConfirmationAction> defaultAction;
    ConfirmationAction escapeAction = ConfirmationAction::Decline;
};

struct UnsavedChangesPromptRequest final
{
    std::string title;
    std::string message;
    std::string question;
    std::string saveText;
    std::string discardText;
    std::string cancelText;
    UnsavedChangesAction defaultAction = UnsavedChangesAction::Cancel;
    UnsavedChangesAction escapeAction = UnsavedChangesAction::Cancel;
};

struct PromptAction final
{
    PromptActionId id;
    std::string text;
    PromptActionRole role = PromptActionRole::Action;
    bool enabled = true;
};

struct NamedActionPromptRequest final
{
    std::string title;
    std::string message;
    std::string informativeText;
    PromptSeverity severity = PromptSeverity::Information;
    std::vector<PromptAction> actions;
    // An absent ID leaves that key behavior to the presentation toolkit.
    std::optional<PromptActionId> defaultActionId;
    std::optional<PromptActionId> escapeActionId;
};

// Prompt cancellation and dismissal are values returned by a successful
// presentation. The outer Domain::Result reports technical/presenter errors.
class UserPromptPort
{
public:
    virtual ~UserPromptPort() = default;

    [[nodiscard]] virtual Domain::Result<void> showMessage(
        const MessagePromptRequest& request
        ) = 0;

    [[nodiscard]] virtual Domain::Result<ConfirmationResponse> confirm(
        const ConfirmationPromptRequest& request
        ) = 0;

    [[nodiscard]] virtual Domain::Result<UnsavedChangesAction>
    confirmUnsavedChanges(
        const UnsavedChangesPromptRequest& request
        ) = 0;

    [[nodiscard]] virtual Domain::Result<std::optional<PromptActionId>>
    chooseAction(
        const NamedActionPromptRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application

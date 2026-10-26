#include "next/ui/qt_user_prompt_presenter.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QThread>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <set>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

using Application::ConfirmationAction;
using Application::ConfirmationButtonRole;
using Application::ConfirmationPromptRequest;
using Application::ConfirmationResponse;
using Application::NamedActionPromptRequest;
using Application::PromptAction;
using Application::PromptActionId;
using Application::PromptActionRole;
using Application::PromptSeverity;
using Application::UnsavedChangesAction;
using Application::UnsavedChangesPromptRequest;

constexpr auto PromptIdProperty = "classmngrNextPromptAutomationId";
constexpr auto PromptKindProperty = "classmngrNextPromptKind";
constexpr auto PromptButtonCommandProperty = "classmngrNextPromptCommand";
constexpr auto PromptButtonActionIdProperty = "classmngrNextPromptActionId";

constexpr auto AcknowledgeCommand = "acknowledge";
constexpr auto AcceptCommand = "accept";
constexpr auto DeclineCommand = "decline";
constexpr auto SaveCommand = "save";
constexpr auto DiscardCommand = "discard";
constexpr auto CancelCommand = "cancel";

QString qString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

PromptAutomationId automationIdFor(const PromptKind kind)
{
    const char* value = "next.prompt.message";
    switch (kind)
    {
        case PromptKind::Message:
            value = "next.prompt.message";
            break;
        case PromptKind::Confirmation:
            value = "next.prompt.confirmation";
            break;
        case PromptKind::UnsavedChanges:
            value = "next.prompt.unsaved-changes";
            break;
        case PromptKind::NamedAction:
            value = "next.prompt.named-action";
            break;
    }

    return *PromptAutomationId::create(value);
}

bool validSeverity(const PromptSeverity severity)
{
    return severity == PromptSeverity::Information
        || severity == PromptSeverity::Warning
        || severity == PromptSeverity::Error;
}

bool validConfirmationAction(const ConfirmationAction action)
{
    return action == ConfirmationAction::Accept
        || action == ConfirmationAction::Decline;
}

bool validUnsavedChangesAction(const UnsavedChangesAction action)
{
    return action == UnsavedChangesAction::Save
        || action == UnsavedChangesAction::Discard
        || action == UnsavedChangesAction::Cancel;
}

bool canPresentPrompt()
{
    auto* application = qobject_cast<QApplication*>(QCoreApplication::instance());
    return application && application->thread() == QThread::currentThread();
}

QMessageBox::Icon iconFor(const PromptSeverity severity)
{
    switch (severity)
    {
        case PromptSeverity::Information:
            return QMessageBox::Information;
        case PromptSeverity::Warning:
            return QMessageBox::Warning;
        case PromptSeverity::Error:
            return QMessageBox::Critical;
    }
    return QMessageBox::NoIcon;
}

PromptSeverity severityFor(const QMessageBox& dialog)
{
    switch (dialog.icon())
    {
        case QMessageBox::Warning:
            return PromptSeverity::Warning;
        case QMessageBox::Critical:
            return PromptSeverity::Error;
        case QMessageBox::NoIcon:
        case QMessageBox::Information:
        case QMessageBox::Question:
            return PromptSeverity::Information;
    }
    return PromptSeverity::Information;
}

QMessageBox::ButtonRole buttonRole(const PromptActionRole role)
{
    switch (role)
    {
        case PromptActionRole::Accept:
            return QMessageBox::AcceptRole;
        case PromptActionRole::Reject:
            return QMessageBox::RejectRole;
        case PromptActionRole::Destructive:
            return QMessageBox::DestructiveRole;
        case PromptActionRole::Action:
            return QMessageBox::ActionRole;
    }
    return QMessageBox::ActionRole;
}

PromptActionRole promptActionRole(const QMessageBox& dialog,
                                  const QAbstractButton* button)
{
    if (!button)
    {
        return PromptActionRole::Action;
    }

    switch (dialog.buttonRole(const_cast<QAbstractButton*>(button)))
    {
        case QMessageBox::AcceptRole:
        case QMessageBox::YesRole:
        case QMessageBox::ApplyRole:
            return PromptActionRole::Accept;
        case QMessageBox::RejectRole:
        case QMessageBox::NoRole:
            return PromptActionRole::Reject;
        case QMessageBox::DestructiveRole:
            return PromptActionRole::Destructive;
        case QMessageBox::ActionRole:
        case QMessageBox::HelpRole:
        case QMessageBox::ResetRole:
        case QMessageBox::InvalidRole:
        case QMessageBox::NRoles:
            return PromptActionRole::Action;
    }
    return PromptActionRole::Action;
}

void configureDialog(
    QMessageBox& dialog,
    const PromptAutomationId& id,
    const PromptKind kind,
    const std::string& title,
    const std::string& message,
    const std::string& details,
    const PromptSeverity severity
    )
{
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setIcon(iconFor(severity));
    dialog.setWindowTitle(qString(title));
    // QMessageBox::setWindowTitle() is a no-op on macOS; write the inherited
    // widget property as well, matching the behavior of the legacy adapter.
    static_cast<QWidget&>(dialog).setWindowTitle(qString(title));
    dialog.setText(qString(message));
    dialog.setTextFormat(Qt::PlainText);
    if (!details.empty())
    {
        dialog.setDetailedText(qString(details));
    }
    dialog.setProperty(PromptIdProperty, qString(id.value()));
    dialog.setProperty(PromptKindProperty, static_cast<int>(kind));
}

void setButtonMetadata(
    QAbstractButton& button,
    const char* command,
    const std::optional<PromptActionId>& actionId = std::nullopt
    )
{
    button.setProperty(PromptButtonCommandProperty, command);
    if (actionId)
    {
        button.setProperty(PromptButtonActionIdProperty, qString(actionId->value()));
    }
}

QPushButton* addButton(
    QMessageBox& dialog,
    const std::string& text,
    const QMessageBox::ButtonRole role,
    const char* command,
    const std::optional<PromptActionId>& actionId = std::nullopt
    )
{
    auto* button = dialog.addButton(qString(text), role);
    setButtonMetadata(*button, command, actionId);
    return button;
}

std::string translatedText(const char* context, const char* source)
{
    return utf8(QCoreApplication::translate(context, source));
}

template <typename Value>
Domain::Result<Value> invalidRequest(const std::string& message)
{
    return Domain::Result<Value>::failure({
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    });
}

template <typename Value>
Domain::Result<Value> presenterUnavailable()
{
    return Domain::Result<Value>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Qt prompts require the application's GUI thread.",
        .recoverable = false
    });
}

QMessageBox* findPrompt(const std::optional<PromptAutomationId>& id)
{
    const auto matches = [&id](QWidget* widget)
    {
        auto* prompt = qobject_cast<QMessageBox*>(widget);
        if (!prompt || !prompt->isVisible())
        {
            return static_cast<QMessageBox*>(nullptr);
        }

        if (id && utf8(prompt->property(PromptIdProperty).toString()) != id->value())
        {
            return static_cast<QMessageBox*>(nullptr);
        }
        return prompt;
    };

    if (QMessageBox* active = matches(QApplication::activeModalWidget()))
    {
        return active;
    }

    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (QMessageBox* prompt = matches(widget))
        {
            return prompt;
        }
    }
    return nullptr;
}

std::optional<PromptAutomationId> promptId(const QMessageBox& dialog)
{
    return PromptAutomationId::create(
        utf8(dialog.property(PromptIdProperty).toString())
        );
}

std::optional<PromptActionId> buttonActionId(const QAbstractButton& button)
{
    const std::string value = utf8(
        button.property(PromptButtonActionIdProperty).toString()
        );
    return value.empty() ? std::nullopt : PromptActionId::create(value);
}

QAbstractButton* buttonForCommand(QMessageBox& dialog, const char* command)
{
    for (QAbstractButton* button : dialog.buttons())
    {
        if (button->property(PromptButtonCommandProperty).toString()
            == QString::fromLatin1(command))
        {
            return button;
        }
    }
    return nullptr;
}

bool clickCommand(
    const PromptAutomationId& id,
    const char* command
    )
{
    const std::optional<PromptAutomationId> requestedId{id};
    if (QMessageBox* prompt = findPrompt(requestedId))
    {
        if (QAbstractButton* button = buttonForCommand(*prompt, command);
            button && button->isEnabled())
        {
            button->click();
            return true;
        }
    }
    return false;
}

bool clickAction(
    const PromptAutomationId& id,
    const PromptActionId& actionId
    )
{
    const std::optional<PromptAutomationId> requestedId{id};
    if (QMessageBox* prompt = findPrompt(requestedId))
    {
        for (QAbstractButton* button : prompt->buttons())
        {
            if (buttonActionId(*button) == actionId && button->isEnabled())
            {
                button->click();
                return true;
            }
        }
    }
    return false;
}

std::optional<std::string> actionIdFor(const ConfirmationAction action)
{
    if (action == ConfirmationAction::Accept)
    {
        return AcceptCommand;
    }
    if (action == ConfirmationAction::Decline)
    {
        return DeclineCommand;
    }
    return std::nullopt;
}

const char* actionIdFor(const UnsavedChangesAction action)
{
    switch (action)
    {
        case UnsavedChangesAction::Save:
            return SaveCommand;
        case UnsavedChangesAction::Discard:
            return DiscardCommand;
        case UnsavedChangesAction::Cancel:
            return CancelCommand;
    }
    return "";
}

} // namespace

std::optional<PromptAutomationId> PromptAutomationId::create(std::string value)
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
    return PromptAutomationId(std::move(value));
}

PromptAutomationId::PromptAutomationId(std::string value)
    : m_value(std::move(value))
{
}

const std::string& PromptAutomationId::value() const noexcept
{
    return m_value;
}

QtUserPromptPresenter::QtUserPromptPresenter(QWidget* parent) noexcept
    : m_parent(parent)
{
}

Domain::Result<void> QtUserPromptPresenter::showMessage(
    const Application::MessagePromptRequest& request
    )
{
    if (!canPresentPrompt())
    {
        return presenterUnavailable<void>();
    }
    if (!validSeverity(request.severity))
    {
        return invalidRequest<void>("Message prompt severity is invalid.");
    }

    QMessageBox dialog(m_parent);
    configureDialog(
        dialog,
        automationIdFor(PromptKind::Message),
        PromptKind::Message,
        request.title,
        request.message,
        request.details,
        request.severity
        );
    QPushButton* acknowledge = addButton(
        dialog,
        request.acknowledgeText.empty()
            ? translatedText("QtUserPromptPresenter", "OK")
            : request.acknowledgeText,
        QMessageBox::AcceptRole,
        AcknowledgeCommand
        );
    dialog.setDefaultButton(acknowledge);
    dialog.setEscapeButton(acknowledge);
    dialog.exec();
    return Domain::Result<void>::success();
}

Domain::Result<ConfirmationResponse> QtUserPromptPresenter::confirm(
    const ConfirmationPromptRequest& request
    )
{
    const bool destructive =
        request.acceptButtonRole == ConfirmationButtonRole::Destructive;
    if (!canPresentPrompt())
    {
        return presenterUnavailable<ConfirmationResponse>();
    }
    if (!validSeverity(request.severity)
        || (request.defaultAction
            && !validConfirmationAction(*request.defaultAction))
        || !validConfirmationAction(request.escapeAction)
        || (request.acceptButtonRole != ConfirmationButtonRole::Accept
            && request.acceptButtonRole != ConfirmationButtonRole::Destructive))
    {
        return invalidRequest<ConfirmationResponse>(
            "Confirmation prompt contains an invalid choice or severity."
            );
    }
    const ConfirmationAction defaultAction = request.defaultAction.value_or(
        destructive ? ConfirmationAction::Decline : ConfirmationAction::Accept
        );

    QMessageBox dialog(m_parent);
    configureDialog(
        dialog,
        automationIdFor(PromptKind::Confirmation),
        PromptKind::Confirmation,
        request.title,
        request.message,
        {},
        request.severity
        );
    QPushButton* accept = addButton(
        dialog,
        request.acceptText.empty()
            ? translatedText(
                "QtUserPromptPresenter",
                destructive ? "Delete" : "Continue"
                )
            : request.acceptText,
        destructive ? QMessageBox::DestructiveRole : QMessageBox::AcceptRole,
        AcceptCommand
        );
    QPushButton* decline = addButton(
        dialog,
        request.declineText.empty()
            ? translatedText("QtUserPromptPresenter", "Cancel")
            : request.declineText,
        QMessageBox::RejectRole,
        DeclineCommand
        );
    dialog.setDefaultButton(
        defaultAction == ConfirmationAction::Accept ? accept : decline
        );
    dialog.setEscapeButton(
        request.escapeAction == ConfirmationAction::Accept ? accept : decline
        );
    dialog.exec();

    if (dialog.clickedButton() == accept)
    {
        return Domain::Result<ConfirmationResponse>::success(
            ConfirmationResponse::Accepted
            );
    }
    if (dialog.clickedButton() == decline)
    {
        return Domain::Result<ConfirmationResponse>::success(
            ConfirmationResponse::Declined
            );
    }
    return Domain::Result<ConfirmationResponse>::success(
        ConfirmationResponse::Dismissed
        );
}

Domain::Result<UnsavedChangesAction>
QtUserPromptPresenter::confirmUnsavedChanges(
    const UnsavedChangesPromptRequest& request
    )
{
    if (!canPresentPrompt())
    {
        return presenterUnavailable<UnsavedChangesAction>();
    }
    if (!validUnsavedChangesAction(request.defaultAction)
        || !validUnsavedChangesAction(request.escapeAction))
    {
        return invalidRequest<UnsavedChangesAction>(
            "Unsaved-changes prompt contains an invalid choice."
            );
    }

    QMessageBox dialog(m_parent);
    configureDialog(
        dialog,
        automationIdFor(PromptKind::UnsavedChanges),
        PromptKind::UnsavedChanges,
        request.title,
        request.message,
        {},
        PromptSeverity::Warning
        );
    dialog.setInformativeText(
        request.question.empty()
            ? qString(translatedText(
                "QObject",
                "Save your changes before leaving?"
                ))
            : qString(request.question)
        );

    QPushButton* save = addButton(
        dialog,
        request.saveText.empty()
            ? translatedText("QObject", "Save")
            : request.saveText,
        QMessageBox::AcceptRole,
        SaveCommand
        );
    QPushButton* discard = addButton(
        dialog,
        request.discardText.empty()
            ? translatedText("QObject", "Discard Changes")
            : request.discardText,
        QMessageBox::DestructiveRole,
        DiscardCommand
        );
    QPushButton* cancel = addButton(
        dialog,
        request.cancelText.empty()
            ? translatedText("QObject", "Cancel")
            : request.cancelText,
        QMessageBox::RejectRole,
        CancelCommand
        );

    const auto buttonForAction = [save, discard, cancel](
                                    const UnsavedChangesAction action
                                    )
    {
        switch (action)
        {
            case UnsavedChangesAction::Save:
                return save;
            case UnsavedChangesAction::Discard:
                return discard;
            case UnsavedChangesAction::Cancel:
                return cancel;
        }
        return cancel;
    };
    dialog.setDefaultButton(buttonForAction(request.defaultAction));
    dialog.setEscapeButton(buttonForAction(request.escapeAction));
    dialog.exec();

    if (dialog.clickedButton() == save)
    {
        return Domain::Result<UnsavedChangesAction>::success(
            UnsavedChangesAction::Save
            );
    }
    if (dialog.clickedButton() == discard)
    {
        return Domain::Result<UnsavedChangesAction>::success(
            UnsavedChangesAction::Discard
            );
    }
    return Domain::Result<UnsavedChangesAction>::success(
        UnsavedChangesAction::Cancel
        );
}

Domain::Result<std::optional<PromptActionId>>
QtUserPromptPresenter::chooseAction(const NamedActionPromptRequest& request)
{
    if (!canPresentPrompt())
    {
        return presenterUnavailable<std::optional<PromptActionId>>();
    }
    if (!validSeverity(request.severity) || request.actions.empty())
    {
        return invalidRequest<std::optional<PromptActionId>>(
            "Named-action prompt requires a valid severity and at least one action."
            );
    }

    std::set<std::string> actionIds;
    for (const PromptAction& action : request.actions)
    {
        if (action.text.empty() || !actionIds.insert(action.id.value()).second
            || (action.role != PromptActionRole::Accept
                && action.role != PromptActionRole::Reject
                && action.role != PromptActionRole::Destructive
                && action.role != PromptActionRole::Action))
        {
            return invalidRequest<std::optional<PromptActionId>>(
                "Named-action prompt contains an invalid or duplicate action."
                );
        }
    }

    const auto enabledActionExists = [&request](
                                         const std::optional<PromptActionId>& id
                                         )
    {
        if (!id)
        {
            return true;
        }
        return std::any_of(
            request.actions.begin(),
            request.actions.end(),
            [&id](const PromptAction& action)
            {
                return action.id == *id && action.enabled;
            }
            );
    };
    if (!enabledActionExists(request.defaultActionId)
        || !enabledActionExists(request.escapeActionId))
    {
        return invalidRequest<std::optional<PromptActionId>>(
            "Named-action default and escape IDs must identify enabled actions."
            );
    }

    QMessageBox dialog(m_parent);
    configureDialog(
        dialog,
        automationIdFor(PromptKind::NamedAction),
        PromptKind::NamedAction,
        request.title,
        request.message,
        {},
        request.severity
        );
    dialog.setInformativeText(qString(request.informativeText));

    std::vector<std::pair<PromptActionId, QPushButton*>> buttons;
    buttons.reserve(request.actions.size());
    for (const PromptAction& action : request.actions)
    {
        QPushButton* button = addButton(
            dialog,
            action.text,
            buttonRole(action.role),
            "named-action",
            action.id
            );
        button->setEnabled(action.enabled);
        buttons.emplace_back(action.id, button);
    }

    const auto buttonForId = [&buttons](
                                 const std::optional<PromptActionId>& id
                                 ) -> QPushButton*
    {
        if (!id)
        {
            return nullptr;
        }
        const auto found = std::find_if(
            buttons.begin(),
            buttons.end(),
            [&id](const auto& entry) { return entry.first == *id; }
            );
        return found == buttons.end() ? nullptr : found->second;
    };
    if (QPushButton* defaultButton = buttonForId(request.defaultActionId))
    {
        dialog.setDefaultButton(defaultButton);
    }
    if (QPushButton* escapeButton = buttonForId(request.escapeActionId))
    {
        dialog.setEscapeButton(escapeButton);
    }
    dialog.exec();

    for (const auto& [actionId, button] : buttons)
    {
        if (dialog.clickedButton() == button)
        {
            return Domain::Result<std::optional<PromptActionId>>::success(
                actionId
                );
        }
    }
    return Domain::Result<std::optional<PromptActionId>>::success(std::nullopt);
}

std::optional<PromptSnapshot> QtPromptAutomationAdapter::activePrompt(
    std::optional<PromptAutomationId> id
    ) const
{
    QMessageBox* dialog = findPrompt(id);
    if (!dialog)
    {
        return std::nullopt;
    }

    const std::optional<PromptAutomationId> promptAutomationId = promptId(*dialog);
    if (!promptAutomationId)
    {
        return std::nullopt;
    }

    PromptSnapshot snapshot{
        .id = *promptAutomationId,
        .kind = static_cast<PromptKind>(
            dialog->property(PromptKindProperty).toInt()
            ),
        .title = utf8(dialog->windowTitle()),
        .text = utf8(dialog->text()),
        .informativeText = utf8(dialog->informativeText()),
        .details = utf8(dialog->detailedText()),
        .severity = severityFor(*dialog),
        .visible = dialog->isVisible()
    };
    const QAbstractButton* defaultButton = dialog->defaultButton();
    const QAbstractButton* escapeButton = dialog->escapeButton();
    for (QAbstractButton* button : dialog->buttons())
    {
        snapshot.actions.push_back(PromptActionSnapshot{
            .id = buttonActionId(*button),
            .text = utf8(button->text()),
            .role = promptActionRole(*dialog, button),
            .enabled = button->isEnabled(),
            .isDefault = button == defaultButton,
            .isEscape = button == escapeButton
        });
    }
    return snapshot;
}

bool QtPromptAutomationAdapter::acknowledge(const PromptAutomationId& id)
{
    return clickCommand(id, AcknowledgeCommand);
}

bool QtPromptAutomationAdapter::chooseConfirmation(
    const PromptAutomationId& id,
    const ConfirmationAction action
    )
{
    if (const auto command = actionIdFor(action))
    {
        return clickCommand(id, command->c_str());
    }
    return false;
}

bool QtPromptAutomationAdapter::chooseUnsavedChanges(
    const PromptAutomationId& id,
    const UnsavedChangesAction action
    )
{
    const char* command = actionIdFor(action);
    return command[0] != '\0' && clickCommand(id, command);
}

bool QtPromptAutomationAdapter::chooseAction(
    const PromptAutomationId& id,
    const PromptActionId& actionId
    )
{
    return clickAction(id, actionId);
}

bool QtPromptAutomationAdapter::activateDefault(const PromptAutomationId& id)
{
    const std::optional<PromptAutomationId> requestedId{id};
    if (QMessageBox* prompt = findPrompt(requestedId))
    {
        if (QPushButton* button = prompt->defaultButton();
            button && button->isEnabled())
        {
            button->click();
            return true;
        }
    }
    return false;
}

bool QtPromptAutomationAdapter::reject(const PromptAutomationId& id)
{
    const std::optional<PromptAutomationId> requestedId{id};
    if (QMessageBox* prompt = findPrompt(requestedId))
    {
        if (QAbstractButton* escapeButton = prompt->escapeButton();
            escapeButton && escapeButton->isEnabled())
        {
            escapeButton->click();
            return true;
        }
        prompt->reject();
        return true;
    }
    return false;
}

bool QtPromptAutomationAdapter::dismiss(const PromptAutomationId& id)
{
    const std::optional<PromptAutomationId> requestedId{id};
    if (QMessageBox* prompt = findPrompt(requestedId))
    {
        prompt->reject();
        return true;
    }
    return false;
}

bool QtPromptAutomationAdapter::capture(
    const PromptAutomationId& id,
    const std::string& path
    )
{
    if (path.empty())
    {
        return false;
    }

    const std::optional<PromptAutomationId> requestedId{id};
    if (QMessageBox* prompt = findPrompt(requestedId))
    {
        const QPixmap image = prompt->grab();
        return !image.isNull() && image.save(qString(path), "PNG");
    }
    return false;
}

} // namespace ClassMngr::Next::Ui

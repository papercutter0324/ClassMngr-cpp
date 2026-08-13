#include "user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QHash>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>

namespace
{

constexpr const char* OkText =
    QT_TRANSLATE_NOOP("QtUserPromptService", "OK");
constexpr const char* DeleteText =
    QT_TRANSLATE_NOOP("QtUserPromptService", "Delete");
constexpr const char* ContinueText =
    QT_TRANSLATE_NOOP("QtUserPromptService", "Continue");
constexpr const char* CancelText =
    QT_TRANSLATE_NOOP("QtUserPromptService", "Cancel");
constexpr const char* SaveText =
    QT_TRANSLATE_NOOP("QObject", "Save");
constexpr const char* DiscardChangesText =
    QT_TRANSLATE_NOOP("QObject", "Discard Changes");
constexpr const char* SaveChangesQuestion =
    QT_TRANSLATE_NOOP(
        "QObject",
        "Save your changes before leaving?"
        );

IUserPromptService* testUserPromptService = nullptr;

QMessageBox::Icon messageBoxIcon(
    PromptSeverity severity
    )
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

QString translatedButtonText(
    const char* text
    )
{
    return QCoreApplication::translate(
        "QtUserPromptService",
        text
        );
}

QString translatedUnsavedText(
    const char* text
    )
{
    return QCoreApplication::translate(
        "QObject",
        text
        );
}

QMessageBox::ButtonRole messageBoxRole(
    PromptActionRole role
    )
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

void configureMessageBox(
    QMessageBox& dialog,
    const PromptRequest& request
    )
{
    dialog.setObjectName(
        request.objectName.isEmpty()
            ? QStringLiteral("classmngrUserPrompt")
            : request.objectName
        );
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setIcon(messageBoxIcon(request.severity));
    dialog.setWindowTitle(request.title);
    dialog.setText(request.message);
    dialog.setTextFormat(Qt::PlainText);

    if (!request.details.isEmpty())
    {
        dialog.setDetailedText(request.details);
    }
}

}

void QtUserPromptService::showMessage(
    const PromptRequest& request
    )
{
    QMessageBox dialog(request.parent);
    configureMessageBox(dialog, request);

    QPushButton* acknowledgeButton = dialog.addButton(
        request.acceptText.isEmpty()
            ? translatedButtonText(OkText)
            : request.acceptText,
        QMessageBox::AcceptRole
        );
    acknowledgeButton->setObjectName(
        QStringLiteral("promptAcceptButton")
        );
    dialog.setDefaultButton(acknowledgeButton);
    dialog.setEscapeButton(acknowledgeButton);
    dialog.exec();
}

void QtUserPromptService::showMessageAsync(
    const PromptRequest& request
    )
{
    auto* dialog = new QMessageBox(request.parent);
    configureMessageBox(*dialog, request);
    auto* acknowledgeButton = dialog->addButton(
        request.acceptText.isEmpty()
            ? translatedButtonText(OkText)
            : request.acceptText,
        QMessageBox::AcceptRole
        );
    acknowledgeButton->setObjectName(
        QStringLiteral("promptAcceptButton")
        );
    dialog->setDefaultButton(acknowledgeButton);
    dialog->setEscapeButton(acknowledgeButton);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

PromptChoice QtUserPromptService::confirm(
    const PromptRequest& request
    )
{
    QMessageBox dialog(request.parent);
    configureMessageBox(dialog, request);

    const QMessageBox::ButtonRole acceptRole =
        request.destructive
            ? QMessageBox::DestructiveRole
            : QMessageBox::AcceptRole;
    QPushButton* acceptButton = dialog.addButton(
        request.acceptText.isEmpty()
            ? request.destructive
                ? translatedButtonText(DeleteText)
                : translatedButtonText(ContinueText)
            : request.acceptText,
        acceptRole
        );
    acceptButton->setObjectName(
        request.destructive
            ? QStringLiteral("promptDestructiveButton")
            : QStringLiteral("promptAcceptButton")
        );

    QPushButton* rejectButton = dialog.addButton(
        request.rejectText.isEmpty()
            ? translatedButtonText(CancelText)
            : request.rejectText,
        QMessageBox::RejectRole
        );
    rejectButton->setObjectName(
        QStringLiteral("promptRejectButton")
        );

    dialog.setDefaultButton(
        request.destructive
            ? rejectButton
            : acceptButton
        );
    dialog.setEscapeButton(rejectButton);
    dialog.exec();

    QAbstractButton* clickedButton = dialog.clickedButton();
    if (clickedButton == acceptButton)
    {
        return request.destructive
            ? PromptChoice::Destructive
            : PromptChoice::Accepted;
    }

    if (clickedButton == rejectButton)
    {
        return PromptChoice::Rejected;
    }

    return PromptChoice::Canceled;
}

UnsavedChangesChoice QtUserPromptService::confirmUnsavedChanges(
    const UnsavedChangesRequest& request
    )
{
    QMessageBox dialog(request.parent);
    configureMessageBox(
        dialog,
        PromptRequest{
            .parent = request.parent,
            .title = request.title,
            .message = request.message,
            .severity = PromptSeverity::Warning
        }
        );
    dialog.setInformativeText(
        request.question.isEmpty()
            ? translatedUnsavedText(SaveChangesQuestion)
            : request.question
        );

    QPushButton* saveButton = dialog.addButton(
        request.saveText.isEmpty()
            ? translatedUnsavedText(SaveText)
            : request.saveText,
        QMessageBox::AcceptRole
        );
    saveButton->setObjectName(QStringLiteral("promptSaveButton"));

    QPushButton* discardButton = dialog.addButton(
        request.discardText.isEmpty()
            ? translatedUnsavedText(DiscardChangesText)
            : request.discardText,
        QMessageBox::DestructiveRole
        );
    discardButton->setObjectName(QStringLiteral("promptDiscardButton"));

    QPushButton* cancelButton = dialog.addButton(
        request.cancelText.isEmpty()
            ? translatedUnsavedText(CancelText)
            : request.cancelText,
        QMessageBox::RejectRole
        );
    cancelButton->setObjectName(QStringLiteral("promptCancelButton"));

    dialog.setDefaultButton(cancelButton);
    dialog.setEscapeButton(cancelButton);
    dialog.exec();

    if (dialog.clickedButton() == saveButton)
    {
        return UnsavedChangesChoice::Save;
    }
    if (dialog.clickedButton() == discardButton)
    {
        return UnsavedChangesChoice::Discard;
    }
    return UnsavedChangesChoice::Cancel;
}

QString QtUserPromptService::chooseAction(
    const ActionPromptRequest& request
    )
{
    QMessageBox dialog(request.prompt.parent);
    configureMessageBox(dialog, request.prompt);
    dialog.setInformativeText(request.informativeText);

    QHash<QAbstractButton*, QString> actionIds;
    QHash<QString, QPushButton*> buttons;
    for (const PromptAction& action : request.actions)
    {
        QPushButton* button = dialog.addButton(
            action.text,
            messageBoxRole(action.role)
            );
        button->setObjectName(
            QStringLiteral("promptAction_%1").arg(action.id)
            );
        button->setEnabled(action.enabled);
        actionIds.insert(button, action.id);
        buttons.insert(action.id, button);
    }

    if (buttons.contains(request.defaultActionId))
    {
        dialog.setDefaultButton(buttons.value(request.defaultActionId));
    }
    if (buttons.contains(request.escapeActionId))
    {
        dialog.setEscapeButton(buttons.value(request.escapeActionId));
    }

    dialog.exec();
    return actionIds.value(dialog.clickedButton());
}

IUserPromptService& DialogServices::prompts()
{
    if (testUserPromptService)
    {
        return *testUserPromptService;
    }

    static QtUserPromptService service;
    return service;
}

void DialogServices::setUserPromptServiceForTesting(
    IUserPromptService* service
    )
{
    testUserPromptService = service;
}

void DialogServices::showInformation(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& details
    )
{
    prompts().showMessage(
        PromptRequest{
            .parent = parent,
            .title = title,
            .message = message,
            .details = details,
            .severity = PromptSeverity::Information
        }
        );
}

void DialogServices::showWarning(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& details
    )
{
    prompts().showMessage(
        PromptRequest{
            .parent = parent,
            .title = title,
            .message = message,
            .details = details,
            .severity = PromptSeverity::Warning
        }
        );
}

void DialogServices::showError(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& details
    )
{
    prompts().showMessage(
        PromptRequest{
            .parent = parent,
            .title = title,
            .message = message,
            .details = details,
            .severity = PromptSeverity::Error
        }
        );
}

PromptChoice DialogServices::confirm(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& acceptText,
    const QString& rejectText,
    bool destructive
    )
{
    return prompts().confirm(
        PromptRequest{
            .parent = parent,
            .title = title,
            .message = message,
            .severity = destructive
                ? PromptSeverity::Warning
                : PromptSeverity::Information,
            .acceptText = acceptText,
            .rejectText = rejectText,
            .destructive = destructive
        }
        );
}

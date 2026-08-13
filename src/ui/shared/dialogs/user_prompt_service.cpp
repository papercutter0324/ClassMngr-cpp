#include "user_prompt_service.h"

#include <QAbstractButton>
#include <QCoreApplication>
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

void configureMessageBox(
    QMessageBox& dialog,
    const PromptRequest& request
    )
{
    dialog.setObjectName(
        QStringLiteral("classmngrUserPrompt")
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

#pragma once

#include <QString>
#include <QVector>

#include <optional>

class QWidget;

enum class PromptSeverity
{
    Information,
    Warning,
    Error
};

enum class PromptChoice
{
    Accepted,
    Rejected,
    Canceled,
    Destructive
};

enum class UnsavedChangesChoice
{
    Save,
    Discard,
    Cancel
};

struct PromptRequest
{
    QWidget* parent = nullptr;
    QString objectName;
    QString automationId;
    QString title;
    QString message;
    QString details;
    PromptSeverity severity = PromptSeverity::Information;
    QString acceptText;
    QString rejectText;
    bool destructive = false;
};

struct PromptSnapshot
{
    QString id;
    QString objectName;
    QString title;
    QString text;
    bool visible = false;
};

class IPromptTestDriver
{
public:
    virtual ~IPromptTestDriver() = default;

    [[nodiscard]] virtual std::optional<PromptSnapshot> activePrompt(
        const QString& id = QString()
        ) const = 0;

    virtual bool accept(
        const QString& id
        ) = 0;

    virtual bool reject(
        const QString& id
        ) = 0;

    virtual bool clickDefault(
        const QString& id
        ) = 0;

    virtual bool capture(
        const QString& id,
        const QString& path
        ) = 0;
};

struct UnsavedChangesRequest
{
    QWidget* parent = nullptr;
    QString title;
    QString message;
    QString question;
    QString saveText;
    QString discardText;
    QString cancelText;
};

enum class PromptActionRole
{
    Accept,
    Reject,
    Destructive,
    Action
};

struct PromptAction
{
    QString id;
    QString text;
    PromptActionRole role = PromptActionRole::Action;
    bool enabled = true;
};

struct ActionPromptRequest
{
    PromptRequest prompt;
    QString informativeText;
    QVector<PromptAction> actions;
    QString defaultActionId;
    QString escapeActionId;
};

class IUserPromptService
{
public:
    virtual ~IUserPromptService() = default;

    virtual void showMessage(
        const PromptRequest& request
        ) = 0;

    virtual void showMessageAsync(
        const PromptRequest& request
        ) = 0;

    [[nodiscard]] virtual PromptChoice confirm(
        const PromptRequest& request
        ) = 0;

    [[nodiscard]] virtual UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest& request
        ) = 0;

    [[nodiscard]] virtual QString chooseAction(
        const ActionPromptRequest& request
        ) = 0;
};

class QtUserPromptService final : public IUserPromptService
{
public:
    void showMessage(
        const PromptRequest& request
        ) override;

    void showMessageAsync(
        const PromptRequest& request
        ) override;

    [[nodiscard]] PromptChoice confirm(
        const PromptRequest& request
        ) override;

    [[nodiscard]] UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest& request
        ) override;

    [[nodiscard]] QString chooseAction(
        const ActionPromptRequest& request
        ) override;
};

class QtPromptTestDriver final : public IPromptTestDriver
{
public:
    [[nodiscard]] std::optional<PromptSnapshot> activePrompt(
        const QString& id = QString()
        ) const override;

    bool accept(
        const QString& id
        ) override;

    bool reject(
        const QString& id
        ) override;

    bool clickDefault(
        const QString& id
        ) override;

    bool capture(
        const QString& id,
        const QString& path
        ) override;
};

namespace DialogServices
{

[[nodiscard]] IUserPromptService& prompts();

[[nodiscard]] IPromptTestDriver& promptTestDriver();

void showInformation(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& details = QString()
    );

void showWarning(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& details = QString()
    );

void showError(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& details = QString()
    );

[[nodiscard]] PromptChoice confirm(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& acceptText = QString(),
    const QString& rejectText = QString(),
    bool destructive = false
    );

void setUserPromptServiceForTesting(
    IUserPromptService* service
    );

}

#pragma once

#include <QString>

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

struct PromptRequest
{
    QWidget* parent = nullptr;
    QString title;
    QString message;
    QString details;
    PromptSeverity severity = PromptSeverity::Information;
    QString acceptText;
    QString rejectText;
    bool destructive = false;
};

class IUserPromptService
{
public:
    virtual ~IUserPromptService() = default;

    virtual void showMessage(
        const PromptRequest& request
        ) = 0;

    [[nodiscard]] virtual PromptChoice confirm(
        const PromptRequest& request
        ) = 0;
};

class QtUserPromptService final : public IUserPromptService
{
public:
    void showMessage(
        const PromptRequest& request
        ) override;

    [[nodiscard]] PromptChoice confirm(
        const PromptRequest& request
        ) override;
};

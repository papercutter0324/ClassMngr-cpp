#include "form_validation_binder.h"

#include "ui/shared/pages/autosave_coordinator.h"

#include <QLabel>
#include <QScrollArea>
#include <QStringList>
#include <QStyle>
#include <QVariant>
#include <QWidget>

#include <utility>

FormValidationBinder::FormValidationBinder(
    AutosaveCoordinator* autosave,
    QScrollArea* scrollArea,
    QObject* parent
    )
    : QObject(parent)
    , m_autosave(autosave)
    , m_scrollArea(scrollArea)
{
}

QLabel* FormValidationBinder::createMessageLabel(QWidget* parent) const
{
    auto* label = new QLabel(parent);
    label->setObjectName(QStringLiteral("formValidationMessage"));
    label->setWordWrap(true);
    label->setVisible(false);
    return label;
}

void FormValidationBinder::registerField(
    QString field,
    QWidget* widget,
    QLabel* messageLabel
    )
{
    field = field.trimmed();
    if (field.isEmpty())
    {
        return;
    }

    if (auto existing = m_bindings.find(field); existing != m_bindings.end())
    {
        clearVisualState(existing.value());
    }

    FieldBinding binding;
    binding.widget = widget;
    binding.messageLabel = messageLabel;
    if (widget)
    {
        binding.originalAccessibleDescription = widget->accessibleDescription();
    }

    m_bindings.insert(field, std::move(binding));
}

void FormValidationBinder::unregisterField(const QString& field)
{
    auto binding = m_bindings.find(field);
    if (binding == m_bindings.end())
    {
        return;
    }

    clearVisualState(binding.value());
    m_bindings.erase(binding);
}

void FormValidationBinder::setValidation(
    ValidationResult validation,
    const MessageFormatter& formatter
    )
{
    for (auto binding = m_bindings.begin(); binding != m_bindings.end(); ++binding)
    {
        clearVisualState(binding.value());
    }

    m_validation = std::move(validation);

    QHash<QString, ValidationIssues> issuesByField;
    for (const ValidationIssue& issue : m_validation.issues())
    {
        if (!issue.field.isEmpty() && m_bindings.contains(issue.field))
        {
            issuesByField[issue.field].append(issue);
        }
    }

    for (auto it = issuesByField.cbegin(); it != issuesByField.cend(); ++it)
    {
        if (auto binding = m_bindings.find(it.key()); binding != m_bindings.end())
        {
            applyVisualState(binding.value(), it.value(), formatter);
        }
    }

    if (m_autosave)
    {
        m_autosave->setValid(!m_validation.hasErrors());
    }

    emit validationChanged(
        m_validation.hasErrors(),
        m_validation.hasWarnings()
        );
}

void FormValidationBinder::clear()
{
    setValidation({});
}

const ValidationResult& FormValidationBinder::validation() const noexcept
{
    return m_validation;
}

bool FormValidationBinder::hasErrors() const noexcept
{
    return m_validation.hasErrors();
}

bool FormValidationBinder::hasWarnings() const noexcept
{
    return m_validation.hasWarnings();
}

bool FormValidationBinder::focusFirstError()
{
    for (const ValidationIssue& issue : m_validation.errors())
    {
        const auto binding = m_bindings.constFind(issue.field);
        if (binding == m_bindings.cend() || !binding->widget)
        {
            continue;
        }

        QWidget* widget = binding->widget;
        if (m_scrollArea)
        {
            m_scrollArea->ensureWidgetVisible(widget, 0, 16);
        }
        widget->setFocus(Qt::OtherFocusReason);
        return true;
    }

    return false;
}

QString FormValidationBinder::messageFor(
    const ValidationIssue& issue,
    const MessageFormatter& formatter
    ) const
{
    if (formatter)
    {
        const QString formatted = formatter(issue).trimmed();
        if (!formatted.isEmpty())
        {
            return formatted;
        }
    }

    const QString& code = issue.code;
    if (code.endsWith(QStringLiteral(".required")))
    {
        return tr("This field is required.");
    }

    if (code == QStringLiteral("validation.length.out_of_bounds"))
    {
        const int minimum = issue.arguments.value(QStringLiteral("minimum")).toInt();
        const int maximum = issue.arguments.value(QStringLiteral("maximum")).toInt();
        if (minimum <= 0)
        {
            return tr("Enter no more than %n characters.", nullptr, maximum);
        }

        return tr(
            "Enter between %1 and %2 characters."
            ).arg(minimum).arg(maximum);
    }

    if (code == QStringLiteral("calendar.date.end_before_start")
        || code == QStringLiteral("calendar.repeat.until_before_start"))
    {
        return tr("The end date must not be before the start date.");
    }

    if (code == QStringLiteral("calendar.time.end_not_after_start")
        || code == QStringLiteral("schedule.time.end_not_after_start"))
    {
        return tr("The end time must be after the start time.");
    }

    if (code == QStringLiteral("calendar.repeat.too_many_occurrences"))
    {
        return tr("Choose an earlier end date for this series.");
    }

    if (code.contains(QStringLiteral("enum"))
        || code.contains(QStringLiteral("invalid_choice"))
        || code.contains(QStringLiteral("not_allowed")))
    {
        return tr("Choose a listed value.");
    }

    if (issue.isWarning())
    {
        return tr("Review this value.");
    }

    return tr("Enter a valid value.");
}

void FormValidationBinder::clearVisualState(FieldBinding& binding) const
{
    if (binding.widget)
    {
        binding.widget->setProperty("formValidationState", QVariant());
        binding.widget->setAccessibleDescription(
            binding.originalAccessibleDescription
            );
        repolish(binding.widget);
    }

    if (binding.messageLabel)
    {
        binding.messageLabel->setText({});
        binding.messageLabel->setAccessibleName({});
        binding.messageLabel->setProperty(
            "formValidationSeverity",
            QVariant()
            );
        binding.messageLabel->setVisible(false);
        repolish(binding.messageLabel);
    }
}

void FormValidationBinder::applyVisualState(
    FieldBinding& binding,
    const ValidationIssues& issues,
    const MessageFormatter& formatter
    ) const
{
    bool hasError = false;
    QStringList messages;
    for (const ValidationIssue& issue : issues)
    {
        hasError = hasError || issue.isError();

        const QString message = messageFor(issue, formatter);
        if (!messages.contains(message))
        {
            messages.append(message);
        }
    }

    const QString message = messages.join(QLatin1Char('\n'));
    const QString severity = hasError
        ? QStringLiteral("error")
        : QStringLiteral("warning");
    const QString accessibleMessage = hasError
        ? tr("Error: %1").arg(message)
        : tr("Warning: %1").arg(message);

    if (binding.widget)
    {
        binding.widget->setProperty("formValidationState", severity);
        binding.widget->setAccessibleDescription(
            binding.originalAccessibleDescription.isEmpty()
                ? accessibleMessage
                : QStringLiteral("%1\n%2")
                      .arg(
                          binding.originalAccessibleDescription,
                          accessibleMessage
                          )
            );
        repolish(binding.widget);
    }

    if (binding.messageLabel)
    {
        binding.messageLabel->setText(message);
        binding.messageLabel->setAccessibleName(accessibleMessage);
        binding.messageLabel->setProperty(
            "formValidationSeverity",
            severity
            );
        binding.messageLabel->setVisible(!message.isEmpty());
        repolish(binding.messageLabel);
    }
}

void FormValidationBinder::repolish(QWidget* widget)
{
    if (!widget || !widget->style())
    {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

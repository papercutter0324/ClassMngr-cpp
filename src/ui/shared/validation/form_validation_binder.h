#pragma once

#include "domain/validation/validation_result.h"

#include <QHash>
#include <QObject>
#include <QPointer>

#include <functional>

class AutosaveCoordinator;
class QLabel;
class QScrollArea;
class QWidget;

// Connects structured domain-validation results to a QWidget form.  Forms own
// the placement of inline labels, while the binder consistently handles field
// state, accessibility, focus, scrolling, and autosave validity.
class FormValidationBinder final : public QObject
{
    Q_OBJECT

public:
    using MessageFormatter = std::function<QString(const ValidationIssue&)>;

    explicit FormValidationBinder(
        AutosaveCoordinator* autosave = nullptr,
        QScrollArea* scrollArea = nullptr,
        QObject* parent = nullptr
        );

    [[nodiscard]] QLabel* createMessageLabel(
        QWidget* parent = nullptr
        ) const;

    void registerField(
        QString field,
        QWidget* widget,
        QLabel* messageLabel = nullptr
        );
    void unregisterField(const QString& field);

    void setValidation(
        ValidationResult validation,
        const MessageFormatter& formatter = {}
        );
    void clear();

    [[nodiscard]] const ValidationResult& validation() const noexcept;
    [[nodiscard]] bool hasErrors() const noexcept;
    [[nodiscard]] bool hasWarnings() const noexcept;

    // Focuses the first registered erroneous field in validation-result order.
    // Returns false when none of the reported errors map to a live widget.
    bool focusFirstError();

signals:
    void validationChanged(bool hasErrors, bool hasWarnings);

private:
    struct FieldBinding
    {
        QPointer<QWidget> widget;
        QPointer<QLabel> messageLabel;
        QString originalAccessibleDescription;
    };

    [[nodiscard]] QString messageFor(
        const ValidationIssue& issue,
        const MessageFormatter& formatter
        ) const;
    void clearVisualState(FieldBinding& binding) const;
    void applyVisualState(
        FieldBinding& binding,
        const ValidationIssues& issues,
        const MessageFormatter& formatter
        ) const;
    static void repolish(QWidget* widget);

    QHash<QString, FieldBinding> m_bindings;
    ValidationResult m_validation;
    AutosaveCoordinator* m_autosave = nullptr;
    QScrollArea* m_scrollArea = nullptr;
};

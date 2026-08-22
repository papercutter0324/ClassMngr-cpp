#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

enum class ValidationSeverity
{
    Warning,
    Error
};

struct ValidationIssue
{
    QString code;
    QString field;

    int row = -1;
    int column = -1;

    ValidationSeverity severity = ValidationSeverity::Error;
    QVariantMap arguments;

    [[nodiscard]] bool isWarning() const noexcept;
    [[nodiscard]] bool isError() const noexcept;
};

using ValidationIssues = QList<ValidationIssue>;

class ValidationResult final
{
public:
    ValidationResult() = default;
    explicit ValidationResult(ValidationIssue issue);
    explicit ValidationResult(ValidationIssues issues);

    [[nodiscard]] const ValidationIssues& issues() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool hasErrors() const noexcept;
    [[nodiscard]] bool hasWarnings() const noexcept;
    [[nodiscard]] ValidationIssues errors() const;
    [[nodiscard]] ValidationIssues warnings() const;
    [[nodiscard]] ValidationIssues forField(const QString& field) const;

    void add(ValidationIssue issue);
    ValidationResult& merge(const ValidationResult& other);

private:
    ValidationIssues m_issues;
};


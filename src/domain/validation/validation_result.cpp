#include "validation_result.h"

#include <utility>

bool ValidationIssue::isWarning() const noexcept
{
    return severity == ValidationSeverity::Warning;
}

bool ValidationIssue::isError() const noexcept
{
    return severity == ValidationSeverity::Error;
}

ValidationResult::ValidationResult(ValidationIssue issue)
{
    m_issues.append(std::move(issue));
}

ValidationResult::ValidationResult(ValidationIssues issues)
    : m_issues(std::move(issues))
{
}

const ValidationIssues& ValidationResult::issues() const noexcept
{
    return m_issues;
}

bool ValidationResult::isValid() const noexcept
{
    return !hasErrors();
}

bool ValidationResult::hasErrors() const noexcept
{
    for (const ValidationIssue& issue : m_issues)
    {
        if (issue.isError())
        {
            return true;
        }
    }

    return false;
}

bool ValidationResult::hasWarnings() const noexcept
{
    for (const ValidationIssue& issue : m_issues)
    {
        if (issue.isWarning())
        {
            return true;
        }
    }

    return false;
}

ValidationIssues ValidationResult::errors() const
{
    ValidationIssues errors;

    for (const ValidationIssue& issue : m_issues)
    {
        if (issue.isError())
        {
            errors.append(issue);
        }
    }

    return errors;
}

ValidationIssues ValidationResult::warnings() const
{
    ValidationIssues warnings;

    for (const ValidationIssue& issue : m_issues)
    {
        if (issue.isWarning())
        {
            warnings.append(issue);
        }
    }

    return warnings;
}

ValidationIssues ValidationResult::forField(const QString& field) const
{
    ValidationIssues issues;

    for (const ValidationIssue& issue : m_issues)
    {
        if (issue.field == field)
        {
            issues.append(issue);
        }
    }

    return issues;
}

void ValidationResult::add(ValidationIssue issue)
{
    m_issues.append(std::move(issue));
}

ValidationResult& ValidationResult::merge(const ValidationResult& other)
{
    m_issues.append(other.m_issues);
    return *this;
}


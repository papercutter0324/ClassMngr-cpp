#include "classmngr/engine/validation_result.h"

#include <utility>

namespace classmngr::engine
{

ValidationResult::ValidationResult(ValidationIssue issue)
{
    m_issues.push_back(std::move(issue));
}

ValidationResult::ValidationResult(std::vector<ValidationIssue> issues)
    : m_issues(std::move(issues))
{
}

const std::vector<ValidationIssue>& ValidationResult::issues() const noexcept
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

std::vector<ValidationIssue> ValidationResult::errors() const
{
    std::vector<ValidationIssue> errors;
    for (const ValidationIssue& issue : m_issues)
    {
        if (issue.isError())
        {
            errors.push_back(issue);
        }
    }

    return errors;
}

std::vector<ValidationIssue> ValidationResult::warnings() const
{
    std::vector<ValidationIssue> warnings;
    for (const ValidationIssue& issue : m_issues)
    {
        if (issue.isWarning())
        {
            warnings.push_back(issue);
        }
    }

    return warnings;
}

void ValidationResult::add(ValidationIssue issue)
{
    m_issues.push_back(std::move(issue));
}

ValidationResult& ValidationResult::merge(const ValidationResult& other)
{
    m_issues.insert(
        m_issues.end(),
        other.m_issues.begin(),
        other.m_issues.end()
        );
    return *this;
}

} // namespace classmngr::engine

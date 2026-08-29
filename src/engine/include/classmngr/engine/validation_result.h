#pragma once

#include <string>
#include <vector>

namespace classmngr::engine
{

enum class ValidationSeverity
{
    Warning,
    Error
};

struct ValidationIssue
{
    std::string code;
    std::string field;
    ValidationSeverity severity = ValidationSeverity::Error;

    [[nodiscard]] bool isWarning() const noexcept
    {
        return severity == ValidationSeverity::Warning;
    }

    [[nodiscard]] bool isError() const noexcept
    {
        return severity == ValidationSeverity::Error;
    }
};

class ValidationResult final
{
public:
    ValidationResult() = default;
    explicit ValidationResult(ValidationIssue issue);
    explicit ValidationResult(std::vector<ValidationIssue> issues);

    [[nodiscard]] const std::vector<ValidationIssue>& issues() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool hasErrors() const noexcept;
    [[nodiscard]] bool hasWarnings() const noexcept;
    [[nodiscard]] std::vector<ValidationIssue> errors() const;
    [[nodiscard]] std::vector<ValidationIssue> warnings() const;

    void add(ValidationIssue issue);
    ValidationResult& merge(const ValidationResult& other);

private:
    std::vector<ValidationIssue> m_issues;
};

} // namespace classmngr::engine

#include "validation_rules.h"

#include <utility>

namespace ValidationRules
{

ValidationIssue issue(
    QString code,
    ValidationLocation location,
    ValidationSeverity severity,
    QVariantMap arguments
    )
{
    return {
        .code = std::move(code),
        .field = std::move(location.field),
        .row = location.row,
        .column = location.column,
        .severity = severity,
        .arguments = std::move(arguments)
    };
}

ValidationResult textLength(
    const QString& value,
    qsizetype minimumLength,
    qsizetype maximumLength,
    ValidationLocation location
    )
{
    if (minimumLength > maximumLength)
    {
        return ValidationResult(issue(
            QStringLiteral("validation.length.invalid_bounds"),
            std::move(location),
            ValidationSeverity::Error,
            {
                {QStringLiteral("minimum"), static_cast<qlonglong>(minimumLength)},
                {QStringLiteral("maximum"), static_cast<qlonglong>(maximumLength)}
            }
            ));
    }

    const qsizetype length = value.size();
    if (length >= minimumLength && length <= maximumLength)
    {
        return {};
    }

    return ValidationResult(issue(
        QStringLiteral("validation.length.out_of_bounds"),
        std::move(location),
        ValidationSeverity::Error,
        {
            {QStringLiteral("length"), static_cast<qlonglong>(length)},
            {QStringLiteral("minimum"), static_cast<qlonglong>(minimumLength)},
            {QStringLiteral("maximum"), static_cast<qlonglong>(maximumLength)}
        }
        ));
}

ValidationResult stringEnumValue(
    const QString& value,
    const QStringList& allowedValues,
    ValidationLocation location
    )
{
    if (allowedValues.contains(value))
    {
        return {};
    }

    QVariantList allowed;
    allowed.reserve(allowedValues.size());
    for (const QString& allowedValue : allowedValues)
    {
        allowed.append(allowedValue);
    }

    return ValidationResult(issue(
        QStringLiteral("validation.enum.invalid_value"),
        std::move(location),
        ValidationSeverity::Error,
        {{QStringLiteral("value"), value},
         {QStringLiteral("allowedValues"), allowed}}
        ));
}

} // namespace ValidationRules

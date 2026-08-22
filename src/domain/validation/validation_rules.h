#pragma once

#include "domain/validation/validation_result.h"

#include <QStringList>
#include <QVariantList>

#include <concepts>
#include <initializer_list>
#include <type_traits>
#include <utility>

struct ValidationLocation
{
    QString field;
    int row = -1;
    int column = -1;
};

namespace ValidationRules
{

[[nodiscard]] ValidationIssue issue(
    QString code,
    ValidationLocation location = {},
    ValidationSeverity severity = ValidationSeverity::Error,
    QVariantMap arguments = {}
    );

[[nodiscard]] ValidationResult textLength(
    const QString& value,
    qsizetype minimumLength,
    qsizetype maximumLength,
    ValidationLocation location = {}
    );

[[nodiscard]] ValidationResult stringEnumValue(
    const QString& value,
    const QStringList& allowedValues,
    ValidationLocation location = {}
    );

template<std::integral Integer>
[[nodiscard]] ValidationResult inclusiveRange(
    Integer value,
    Integer minimum,
    Integer maximum,
    ValidationLocation location = {}
    )
{
    const auto toVariant = [](Integer number)
    {
        if constexpr (std::is_signed_v<Integer>)
        {
            return QVariant::fromValue(static_cast<qlonglong>(number));
        }

        return QVariant::fromValue(static_cast<qulonglong>(number));
    };

    if (minimum > maximum)
    {
        return ValidationResult(issue(
            QStringLiteral("validation.range.invalid_bounds"),
            std::move(location),
            ValidationSeverity::Error,
            {
                {QStringLiteral("minimum"), toVariant(minimum)},
                {QStringLiteral("maximum"), toVariant(maximum)}
            }
            ));
    }

    if (value >= minimum && value <= maximum)
    {
        return {};
    }

    return ValidationResult(issue(
        QStringLiteral("validation.range.out_of_bounds"),
        std::move(location),
        ValidationSeverity::Error,
        {
            {QStringLiteral("value"), toVariant(value)},
            {QStringLiteral("minimum"), toVariant(minimum)},
            {QStringLiteral("maximum"), toVariant(maximum)}
        }
        ));
}

template<typename Enum>
requires std::is_enum_v<Enum>
[[nodiscard]] ValidationResult enumValue(
    Enum value,
    std::initializer_list<Enum> allowedValues,
    ValidationLocation location = {}
    )
{
    for (const Enum allowedValue : allowedValues)
    {
        if (value == allowedValue)
        {
            return {};
        }
    }

    using Underlying = std::underlying_type_t<Enum>;
    QVariantList allowed;
    allowed.reserve(static_cast<qsizetype>(allowedValues.size()));
    for (const Enum allowedValue : allowedValues)
    {
        allowed.append(QVariant::fromValue(
            static_cast<qlonglong>(static_cast<Underlying>(allowedValue))
            ));
    }

    return ValidationResult(issue(
        QStringLiteral("validation.enum.invalid_value"),
        std::move(location),
        ValidationSeverity::Error,
        {
            {QStringLiteral("value"), QVariant::fromValue(
                 static_cast<qlonglong>(static_cast<Underlying>(value))
                 )},
            {QStringLiteral("allowedValues"), allowed}
        }
        ));
}

} // namespace ValidationRules

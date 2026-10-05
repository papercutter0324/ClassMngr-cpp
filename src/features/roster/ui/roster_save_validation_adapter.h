#pragma once

#include "domain/validation/validation_result.h"
#include "next/application/roster_save_preparation.h"
#include "ui/shared/qt_text_adapter.h"

#include <QVariantList>
#include <type_traits>

namespace RosterSaveValidationAdapter
{
inline QString text(const std::u16string_view value)
{
    return Ui::QtTextAdapter::fromUtf16String(value);
}

inline ValidationResult validation(
    const ClassMngr::Next::Application::RosterSavePreparation& prepared)
{
    using namespace ClassMngr::Next::Application;
    ValidationResult result;
    for (const auto& source : prepared.issues)
    {
        QVariantMap arguments;
        for (const auto& [key, argument] : source.arguments)
        {
            arguments.insert(QString::fromStdString(key), std::visit([&](const auto& value) -> QVariant
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::u16string>)
                    return text(value);
                else if constexpr (std::is_same_v<Value, std::int64_t>)
                {
                    if (key == "maximumLength" || key == "duplicateColumn")
                        return static_cast<int>(value);
                    return static_cast<qlonglong>(value);
                }
                else
                {
                    QVariantList rows;
                    for (const int row : value)
                        rows.append(row);
                    return rows;
                }
            }, argument));
        }
        result.add({.code = QString::fromStdString(source.code), .field = text(source.field),
            .row = source.row, .column = source.column,
            .severity = source.severity == RosterSaveIssueSeverity::Error
                ? ValidationSeverity::Error : ValidationSeverity::Warning,
            .arguments = std::move(arguments)});
    }
    return result;
}
} // namespace RosterSaveValidationAdapter

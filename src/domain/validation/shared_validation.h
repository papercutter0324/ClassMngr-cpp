#pragma once

#include "domain/validation/validation_result.h"
#include "domain/validation/validation_rules.h"

#include <QList>
#include <QStringList>

namespace SharedValidation
{

[[nodiscard]] ValidationResult englishName(
    const QString& value,
    ValidationLocation location = {}
    );

[[nodiscard]] ValidationResult koreanName(
    const QString& value,
    ValidationLocation location = {}
    );

[[nodiscard]] ValidationResult duplicateNamePairs(
    const QList<QStringList>& rows,
    int englishColumn,
    int koreanColumn,
    QString englishField,
    QString koreanField
    );

[[nodiscard]] ValidationResult weekday(
    const QString& value,
    ValidationLocation location = {}
    );

[[nodiscard]] ValidationResult time(
    const QString& value,
    ValidationLocation location = {}
    );

[[nodiscard]] ValidationResult timeOrder(
    const QString& start,
    const QString& end,
    ValidationLocation startLocation = {},
    ValidationLocation endLocation = {}
    );

[[nodiscard]] ValidationResult color(
    const QString& value,
    ValidationLocation location = {}
    );

[[nodiscard]] ValidationResult fileName(
    const QString& value,
    ValidationLocation location = {}
    );

} // namespace SharedValidation


#pragma once

#include "domain/models/class_info.h"
#include "domain/validation/validation_result.h"

#include <QList>
#include <QString>

class ClassTimeValidator final
{
public:
    // Canonicalizes recognized weekday/time values while leaving malformed
    // values intact for validate() to report.
    [[nodiscard]] static ClassTime normalized(const ClassTime& time);

    [[nodiscard]] static ValidationResult validate(
        const QList<ClassTime>& times,
        const QString& fieldPrefix
        );
};

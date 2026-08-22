#pragma once

#include "domain/models/class_info.h"
#include "domain/validation/validation_result.h"

class ClassInfoValidator final
{
public:
    [[nodiscard]] static ClassInfo normalized(const ClassInfo& info);
    [[nodiscard]] static ValidationResult validate(const ClassInfo& info);
    [[nodiscard]] static ValidationResult validateNotes(
        int classId,
        const QString& notes,
        const QString& timeFillerActivities
        );
};

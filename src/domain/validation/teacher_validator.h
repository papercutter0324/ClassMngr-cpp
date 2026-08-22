#pragma once

#include "domain/models/teacher.h"
#include "domain/validation/validation_result.h"

#include <QString>

class TeacherValidator final
{
public:
    [[nodiscard]] static Teacher normalized(const Teacher& teacher);
    [[nodiscard]] static QString normalizedPhoneNumber(const QString& value);
    [[nodiscard]] static ValidationResult validate(const Teacher& teacher);
};
